#include <iostream>
#include <chrono>
#include <random>
#include <cmath>
#include <mpi.h>
#include <ff/ff.hpp>

constexpr int TASK_TAG=1;
constexpr int ACK_TAG=2;
constexpr int EOS_TAG=4;

constexpr int EXEC_TIME=1000;
constexpr std::uint64_t MT_PRIME_SEED = 18446744073709551557ULL;

// global variables ------------------------------------
int   total_tasks;       // number of tasks
int   task_size;         // bytes per task
int   batch_tasks;       // batch size (E->W and W->C have the same batch_tasks)
int   th_x_worker;       // number of threads used by OpenMP within a single Worker
float psuccess=0.5;      // probability of success in the geometric distribution, small values
                         // produce highly skewed distribution
double t_start, t_end;   // to measure the elapsed time
int size;                // to store the number of processes in total
// -----------------------------------------------------

#define CHECK_ERROR(err) do {								\
	if (err != MPI_SUCCESS) {								\
		char errstr[MPI_MAX_ERROR_STRING];					\
		int errlen=0;										\
		MPI_Error_string(err,errstr,&errlen);				\
		std::cerr											\
			<< "MPI error code " << err						\
			<< " (" << std::string(errstr,errlen) << ")"	\
			<< " line: " << __LINE__ << "\n";				\
		MPI_Abort(MPI_COMM_WORLD, err);						\
		std::abort();										\
	}														\
} while(0)


// simulate CPU intensive work
static inline float active_delay(int usecs) {
  float x = 1.25f;
  auto start = std::chrono::high_resolution_clock::now();
  auto end   = false;
  while(!end) {
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    x *= sin(x) / atan(x) * tanh(x) * sqrt(x);
    if(usec>=usecs)
      end = true;
  }
  return x;
}


// Nodes for implementing the parts -----------------

struct Emitter : public ff::ff_node {
    void* svc(void*){
        std::mt19937_64 rng(MT_PRIME_SEED);
        std::geometric_distribution dist(psuccess);
        for(int i = 0; i < total_tasks; i++){
            char* out = new char[task_size];
            int v = dist(rng);
			std::memcpy(out, &v, sizeof(int));

            ff_send_out(out);
        }
        return this->EOS;
    }

};

struct Worker : public ff::ff_node {
    void* svc(void* in){
        int workload = *reinterpret_cast<int*>(in);
        if (workload) active_delay(workload*EXEC_TIME);
        return in;
    }
};

struct Collector : public ff::ff_node {
    int collected = 0;
    void* svc(void* in){
        delete [] reinterpret_cast<char*>(in);
        ++collected;
        return this->GO_ON;
    }

    void svc_end(){
        t_end = MPI_Wtime();
        std::printf("MPI,%d,%d,%d,%d,%d,%f\n", collected, task_size, batch_tasks, size-2, th_x_worker, t_end-t_start);
    }
};

// Nodes for implementing communications -------------


struct Receiver : public ff::ff_monode {
    int error;
    int workersProcesses = 0;
    int eos_received = 0;
    Receiver(int workersProcesses = 0) : workersProcesses(workersProcesses) {}

    inline void sendACK(int rank){
        if (workersProcesses == 0) rank = 0; // if i'm the reciver of a worker send ack to rank 0 the emitter, otherwise as specified
        int error = MPI_Send(nullptr, 0, MPI_BYTE, rank, ACK_TAG, MPI_COMM_WORLD);
		CHECK_ERROR(error);
    }

    void* svc(void*){
        char* recvBuffer = new char[task_size*batch_tasks];
        while(true){
            MPI_Status st;
            error = MPI_Probe(workersProcesses == 0 ? 0 : MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
		    CHECK_ERROR(error);
            if (st.MPI_TAG == EOS_TAG) { // End-Of-Stream
                error = MPI_Recv(nullptr,0,MPI_BYTE, st.MPI_SOURCE, EOS_TAG,
							 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			    CHECK_ERROR(error);
                if (++eos_received >= workersProcesses)
                    return this->EOS;
                else
                    continue;
            }

            // in the original version from massimo, the sendACK procedure is placed here
            sendACK(st.MPI_SOURCE);
            int   recv_count;
            MPI_Get_count(&st, MPI_BYTE, &recv_count);
            int tasks_in_batch = recv_count / task_size;
            error = MPI_Recv(recvBuffer, recv_count, MPI_BYTE, st.MPI_SOURCE, TASK_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		    CHECK_ERROR(error);

            //sendACK(st.MPI_SOURCE);

            for(int i = 0; i < tasks_in_batch; i++){
                char* out = new char[task_size];
                memcpy(out, recvBuffer+i*task_size, task_size);
                ff_send_out(out);
            }
        }
    }
};

struct Sender : public ff::ff_minode {
    int workersProcesses;
    char* sendBuff = nullptr;
    int count = 0;
    int error;

    int last_send_idx = -1;
    Sender(int workersP = 0) : workersProcesses(workersP) {
        sendBuff = new char[task_size*batch_tasks];
    }

    inline void sendToRank(int rank){
        if (!workersProcesses) rank = size-1;
        error = MPI_Send(sendBuff, count*task_size, MPI_BYTE, rank , TASK_TAG, MPI_COMM_WORLD);
        CHECK_ERROR(error);
        count = 0;
    }

    void* svc(void* in){
        memcpy(sendBuff + count*task_size, in, task_size);
        count += 1;
        delete [] reinterpret_cast<char*>(in);
        
        if (count == batch_tasks){ // here i should send out the batch
            if ((workersProcesses == 0 && last_send_idx == -1) || last_send_idx+1 < workersProcesses ) // here i sending the first batch to all worker in the transient phase
                sendToRank(++last_send_idx+1);
            else { // here we are at steady state
                MPI_Status st;
                error = MPI_Recv(nullptr,0,MPI_BYTE, MPI_ANY_SOURCE, ACK_TAG, MPI_COMM_WORLD, &st);
		        CHECK_ERROR(error);
                int ready_rank = st.MPI_SOURCE;
                sendToRank(ready_rank);
            }
        }
        return this->GO_ON;
    }

    void svc_end(){
        if (count != 0){ // flush the buffer in output to an available worker
            if (last_send_idx+1 < workersProcesses)
                sendToRank(++last_send_idx+1);
            else {
                MPI_Status st;
                error = MPI_Recv(nullptr,0,MPI_BYTE, MPI_ANY_SOURCE, ACK_TAG, MPI_COMM_WORLD, &st);
                CHECK_ERROR(error);
                int ready_rank = st.MPI_SOURCE;
                sendToRank(ready_rank);

                // in order to receive also all the acks, receive the last ack
                error = MPI_Recv(nullptr,0,MPI_BYTE, ready_rank, ACK_TAG, MPI_COMM_WORLD, &st);
                CHECK_ERROR(error);
            }
        }
        if (workersProcesses)
            for(int i = 0; i < workersProcesses; i++) // send the EOS to all of them
                MPI_Send(nullptr, 0, MPI_BYTE, i+1, EOS_TAG, MPI_COMM_WORLD);
        else
            MPI_Send(nullptr, 0, MPI_BYTE, size-1, EOS_TAG, MPI_COMM_WORLD); // send EOS to the collector process

    }
};

// ---------------------------------------------------



void usage(char *argv[]) {
	std::printf("usage: mpirun -n <P> %s total_tasks bytes_x_task batch_size thread_x_worker [prob=%f]\n",
				argv[0], psuccess);
}

int main(int argc, char**argv){
    if (argc < 5) {
		usage(argv);
		return -1;
	}

    total_tasks   = std::stoll(argv[1]);
    task_size     = std::atoi(argv[2]);
    batch_tasks   = std::atoi(argv[3]);
	if (task_size<4) task_size=4;
	th_x_worker   = std::atoi(argv[4]);

	if (argc == 6) {
		auto p  = std::atof(argv[5]);
		if (p>=1 || p<=0) {
			std::printf("Invalid success probability, it should be in (0,1)\n");
			return -1;
		}
		psuccess = p;
	}

	int provided;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	if (provided < MPI_THREAD_MULTIPLE) {
		std::printf("MPI does not provide required threading support\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
		std::abort();
	}

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 3) {
        if (rank==0) {
            std::printf("Too few processes, at least three are required\n");
			usage(argv);
		}
        MPI_Abort(MPI_COMM_WORLD,-1);
		return -1;
    }

    MPI_Barrier(MPI_COMM_WORLD);
	t_start = MPI_Wtime();

    if (rank == 0){
        // emitter
        auto pipe = ff::ff_Pipe(new Emitter, new Sender(size-2));
        pipe.setXNodeInputQueueLength(1, true);
        pipe.setXNodeOutputQueueLength(1, true);
        pipe.run_and_wait_end();
    } else if (rank == size-1) {
        // collector
        auto pipe = ff::ff_Pipe(new Receiver(size-2), new Collector);
        pipe.run_and_wait_end();
    } else {
        // worker
        ff::ff_farm f;
        f.add_emitter(new Receiver);
        std::vector<ff::ff_node*> workers;
        for(int i = 0; i < th_x_worker; i++) 
            workers.push_back(new Worker);
        f.add_workers(workers);
        f.add_collector(new Sender);
        f.set_scheduling_ondemand();
        f.run_and_wait_end();
    }

    MPI_Finalize();
    return 0;
}