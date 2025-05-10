#include <mpi.h>
#include <omp.h>

#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <chrono>
#include <cstring>
#include <cmath>
#include <cassert>

constexpr int TASK_TAG=1;
constexpr int ACK_TAG=2;
constexpr int RESULT_TAG=3;
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

// per-Worker data
struct Buf {
	char*         data;
	MPI_Request   req;
	bool          in_use;
};


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


// rank 0
void Emitter(int size) {
	int error;
    int num_workers = size - 2;
    int tasks_sent = 0;
    int eos_sent   = 0;

    std::mt19937_64 rng(MT_PRIME_SEED);
    std::geometric_distribution dist(psuccess);

	// allocate and copy data into the send buffer until the batch
	// is filled or there are no more tasks, then sends the batch
	// to Worker with id rank
	auto prepare_and_send_batch= [&](Buf &buf, int rank) {
		int count=0;
		for(int i=0;i<batch_tasks &&  tasks_sent<total_tasks; i++, tasks_sent++){
			// copy garbage into the buffer
			char *datatask = new char[task_size];
			std::memcpy(buf.data + i*task_size, datatask, task_size);

			// pertask workload
			int v = dist(rng);
			std::memcpy(buf.data + i*task_size, &v, sizeof(int));

			++count;
			delete [] datatask;
		}
		int error = MPI_Send(buf.data, count*task_size,
							  MPI_BYTE, rank,
							  TASK_TAG, MPI_COMM_WORLD);
		CHECK_ERROR(error);
		buf.in_use = true;
	};

	// pre-allocate two buffers for each Worker
    std::vector<std::array<Buf,2>> bufs(num_workers);
    for(int i=0;i<num_workers;i++){
		bufs[i][0].data   = new char[task_size * batch_tasks];
		bufs[i][1].data   = new char[task_size * batch_tasks];
		bufs[i][0].in_use = bufs[i][1].in_use = false;
		assert(bufs[i][0].data && bufs[i][1].data);
    }

	// keep track of which buffer to use at each round
    std::vector<int> whichbuffer(num_workers,0);

	// initially all worker are ready
	// prepare and send the batch
	for(int i=1;i<=num_workers;++i) {
		prepare_and_send_batch(bufs[i-1][0], i);
		whichbuffer[i-1] ^= 1; // switch buffer
	}

    while(eos_sent < num_workers) {
        MPI_Status st;
        error = MPI_Recv(nullptr,0,MPI_BYTE, MPI_ANY_SOURCE, ACK_TAG,
						 MPI_COMM_WORLD, &st);
		CHECK_ERROR(error);
        int ready_rank = st.MPI_SOURCE;

        if ( tasks_sent < total_tasks ) {
            int buf_id = whichbuffer[ready_rank-1];
			auto &buf = bufs[ready_rank-1][buf_id];

            // if the buffer is still in use, wait for send completion
            /*if (buf.in_use) {
                error = MPI_Wait(&buf.req, MPI_STATUS_IGNORE);
				CHECK_ERROR(error);
                buf.in_use = false;
            }*/
			prepare_and_send_batch(buf, ready_rank);
            whichbuffer[ready_rank-1] ^= 1;
        }
        else {
            // sending EOS to stop the Worker
            error = MPI_Send(nullptr,0,MPI_BYTE, ready_rank, EOS_TAG, MPI_COMM_WORLD);
			CHECK_ERROR(error);
            eos_sent++;
        }
    }
    // cleanup
    for(int i=0;i<num_workers;i++){
		if (bufs[i][0].in_use) {
			//MPI_Wait(&bufs[i][0].req, MPI_STATUS_IGNORE);
			delete[] bufs[i][0].data;
		}
		if (bufs[i][1].in_use) {
			//MPI_Wait(&bufs[i][1].req, MPI_STATUS_IGNORE);
			delete[] bufs[i][1].data;
        }
    }
}

// ranks form 1 to size-2
void Worker(int rank, int size) {
	int error;
    int emitter = 0;
    int collector = size-1;

	// it sends an empty message to the Emitter to notify that I'm just
	// received a task and I'm ready for the next one (so to overlap
	// computation and message transfer)
	auto send_ready= [emitter]() {
		int error = MPI_Send(nullptr, 0, MPI_BYTE, emitter, ACK_TAG, MPI_COMM_WORLD);
		CHECK_ERROR(error);
	};

	// one receive buffer and two send buffers
    std::vector<char> recv_buf(task_size * batch_tasks);
    std::vector<char> send_buf[2] = {
		std::vector<char>(task_size * batch_tasks),
		std::vector<char>(task_size * batch_tasks)
	};

	MPI_Request send_reqs[2] = {MPI_REQUEST_NULL, MPI_REQUEST_NULL};
    int curr = 0; // current buffer index
    int prev = 1; // index of the previously used buffer
    while(true) {
		MPI_Status st;
        error = MPI_Probe(emitter, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
		CHECK_ERROR(error);
        if (st.MPI_TAG == EOS_TAG) { // End-Of-Stream
            error = MPI_Recv(nullptr,0,MPI_BYTE, emitter, EOS_TAG,
							 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			CHECK_ERROR(error);
            break;
        }

		// sending ready message to the Emitter
		send_ready();

		int   recv_count;
        MPI_Get_count(&st, MPI_BYTE, &recv_count);
        int tasks_in_batch = recv_count / task_size;
        error = MPI_Recv(recv_buf.data(), recv_count, MPI_BYTE,
						 emitter, TASK_TAG,
						 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		CHECK_ERROR(error);

		// do I have to wait for send completion?
		/*if (send_reqs[curr] != MPI_REQUEST_NULL) {
			error = MPI_Wait(&send_reqs[curr], MPI_STATUS_IGNORE);
			CHECK_ERROR(error);
		}*/
        std::vector<char*> tasks(tasks_in_batch);
        for(int i = 0; i < tasks_in_batch; i++){
            tasks[i] = new char[task_size];
            std::memcpy(tasks[i], recv_buf.data()+task_size*i, task_size);
        }
#if 0
		// compute in parallel all tasks in the batch
#pragma omp parallel num_threads(th_x_worker)
		{
#pragma omp single nowait
			{
#pragma omp taskloop
				for(int i=0;i<tasks_in_batch;i++){

					//std::printf("Worker%d thread=%d executing a task\n", rank, omp_get_thread_num());

					// simulate work
					int workload;
					std::memcpy(&workload, tasks[i], sizeof(workload));
					if (workload) active_delay(workload*EXEC_TIME);
				}
#pragma omp taskwait
			}
		}
#else
#pragma omp parallel for schedule(dynamic) num_threads(th_x_worker)
				for(int i=0;i<tasks_in_batch;i++){

                                        //std::printf("Worker%d thread=%d executing a task\n", rank, omp_get_thread_num());

                                        // simulate work
                                        int workload;
                                        std::memcpy(&workload, tasks[i], sizeof(workload));
                                        if (workload) active_delay(workload*EXEC_TIME);
                                       
                                 
                                }

#endif
		for(int i = 0; i < tasks_in_batch; i++){
			// copying the result in the send buffer
			std::memcpy(send_buf[curr].data() + i*task_size,
			tasks[i],
			task_size);
			
			delete [] tasks[i];
		}

		error = MPI_Send(send_buf[curr].data(), tasks_in_batch*task_size,
						  MPI_BYTE, collector,
						  RESULT_TAG, MPI_COMM_WORLD);
		CHECK_ERROR(error);

		std::swap(curr,prev);  // swap the send buffer indexes
    }
	// Before exiting, make sure any in-flight send completes
	/*if (send_reqs[0] != MPI_REQUEST_NULL) {
		error = MPI_Wait(&send_reqs[0], MPI_STATUS_IGNORE);
		CHECK_ERROR(error);
	}
	if (send_reqs[1] != MPI_REQUEST_NULL) {
		MPI_Wait(&send_reqs[1], MPI_STATUS_IGNORE);
		CHECK_ERROR(error);
	}*/
}

// rank size-1
void Collector(int num_workers) {
	int error;
	std::vector<int> tasks_per_worker(num_workers,0);

    int buff_size = task_size * batch_tasks;
    char* buffers = new char[task_size * batch_tasks];

    int total_received = 0;

    MPI_Status st;
    while (true) {
        int idx;
		error = MPI_Recv(buffers, buff_size, MPI_BYTE, MPI_ANY_SOURCE, RESULT_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		CHECK_ERROR(error);

		int source = st.MPI_SOURCE;
        int got = 0;
        MPI_Get_count(&st, MPI_BYTE, &got);
		got = got / task_size;
        total_received += got;
        tasks_per_worker[source-1] += got;

        std::vector<char*> tasks(got);
        for(int i = 0; i < got; i++){
            tasks[0] = new char[task_size];
            std::memcpy(tasks[0], buffers+task_size*i, task_size);
        }
        tasks.size(); // just to not fuse the two loops togheter
        for(int i = 0; i < got; i++)
            delete [] tasks[i];

		if (total_received >= total_tasks) break;
    }
	double t_end = MPI_Wtime();

#ifdef DEBUG_SCHEDULING
	std::printf("Collector received %d tasks\n", total_received);
	for(int i=0;i<num_workers;++i) {
		std::printf("%d: %d\n", i+1, tasks_per_worker[i]);
	}
#endif

    std::printf("MPI,%d,%d,%d,%d,%d,%f\n", total_received, task_size, batch_tasks, size-2, th_x_worker, t_end-t_start);
    //std::printf("Elapsed time %f sec\n", t_end-t_start);

    delete [] buffers;
}

void usage(char *argv[]) {
	std::printf("usage: mpirun -n <P> %s total_tasks bytes_x_task batch_size thread_x_worker [prob=%f]\n",
				argv[0], psuccess);
}

int main(int argc, char **argv) {
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

	MPI_Barrier(MPI_COMM_WORLD); // needed to measure exec time properly
	t_start = MPI_Wtime();

	if (rank==0) Emitter(size);
	else {
		if (rank==(size-1)) Collector(size-2);
		else
			Worker(rank,size);
	}

    MPI_Finalize();
    return 0;
}