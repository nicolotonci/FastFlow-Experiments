#include <iostream>
#include <random>
#include <chrono>
#include <ff/dff.hpp>
#include <mpi.h>

using namespace ff;

constexpr int EXEC_TIME=1000;
constexpr std::uint64_t MT_PRIME_SEED = 18446744073709551557ULL;

// global variables ------------------------------------
int   total_tasks;       // number of tasks
int   task_size;         // bytes per task
int   batch_tasks;       // batch size (E->W and W->C have the same batch_tasks)
int   th_x_worker;       // number of threads used by OpenMP within a single Worker
float psuccess=0.5;      // probability of success in the geometric distribution, small values
                         // produce highly skewed distribution
double t_start=0;
int worker_groups;       // to store the number of worker processes
// -----------------------------------------------------

struct Task {
    char* m = nullptr;
    Task() {} // default constructor
    Task(int workload){
        m = new char[task_size];
        std::memcpy(m, &workload, sizeof(int));
    }

    ~Task() {
        if (m) delete [] m;
    }

    // serialization functions ---------------------------
    std::tuple<char*, size_t, bool> serialize(){
        return {this->m, task_size, false};
    }

    bool deserialize(char* buffer, size_t sz){
        this->m = buffer;
        return false;
    }
    // ---------------------------------------------------
};

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

class Emitter : public ff_monode_t<Task> {

    Task* svc(Task*){
        // initialize random number generator
        std::mt19937_64 rng(MT_PRIME_SEED);
        std::geometric_distribution dist(psuccess);
        
        // generate and send tasks
        for(int i = 0; i < total_tasks; i++)
            ff_send_out(new Task(dist(rng)*EXEC_TIME));
        
        return this->EOS; 
    }
};

class Worker : public ff_node_t<Task> {
    Task* svc(Task* t){
        int workload = *reinterpret_cast<int*>(t->m);
        if (workload) active_delay(workload);
        return t;
    }
};

class Collector : public ff_minode_t<Task> {
    std::vector<int> processed;

    Task* svc(Task* t){
        delete t;
        ++processed[this->get_channel_id() / th_x_worker];
        return this->GO_ON;
    }

    void svc_end(){
        auto t_end = getusec();
        
        int total = 0;
        for(unsigned int i = 0; i < processed.size(); i++){
            total += processed.at(i);
#ifdef DEBUG_SCHEDULING
            std::printf("Group %d processed %d tasks\n", i, processed[i]);
#endif
        }
        std::printf("FF,%d,%d,%d,%d,%d,%f\n", total, task_size, batch_tasks, worker_groups, th_x_worker, (t_end-t_start)/1000000.0);
        //std::printf("Collector processed %d items.\n", total);
        //std::printf("Elapsed time %f sec\n", (t_end-t_start)/1000000.0);
    }

public:
    Collector(int groups){
        processed = std::vector<int>(groups, 0);
    }
};

int main(int argc, char** argv){
    DFF_Init(argc, argv);
    if (argc < 5){
        std::printf("usage: mpirun -n <P> %s total_tasks bytes_x_task batch_size thread_x_worker [prob=%f]\n", argv[0], psuccess);
        return -1;
    }

    total_tasks   = std::stoll(argv[1]);
    task_size     = std::atoi(argv[2]);
    batch_tasks   = std::atoi(argv[3]);
    ff::setBatchSize(batch_tasks); // set the batch size programmatically
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

    ff_pipeline outerPipe;
    ff_a2a a2a;
    Emitter e;
    std::vector<Worker*> workers;
    

    int groups;
    MPI_Comm_size(MPI_COMM_WORLD, &groups);
    worker_groups = groups - 2;

    Collector c(worker_groups);

    a2a.add_firstset<Emitter>({&e}, 1); // set the ondemand to 1
    for (int j = 0; j < worker_groups; j++){
        auto g = createGroup("W"+std::to_string(j));
        for(int i = 0; i < th_x_worker; i++){
            Worker* w = new Worker;
            workers.push_back(w);
            g << w;
        }
    }

    e.createGroup("E");
    c.createGroup("C");

    a2a.add_secondset<Worker>(workers, true);
    outerPipe.add_stage(&a2a);
    outerPipe.add_stage(&c);

    MPI_Barrier(MPI_COMM_WORLD);
    t_start = getusec();

    return outerPipe.run_and_wait_end();;
}