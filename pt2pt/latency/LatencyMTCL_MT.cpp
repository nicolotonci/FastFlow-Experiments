#include <iostream>
//#include <ff/ff.hpp>
#define MTCL_DISABLE_COLLECTIVES
#define SINGLE_IO_THREAD
#define NO_MTCL_MULTITHREADED
#include <ff/dff.hpp>
#include <mtcl.hpp>
#include <mpi.h>
#include <mutex>
#include "../../utils/synchronization.hpp"

#define MANUAL_SERIALIZATION
#include "../../utils/payload.hpp"

using namespace ff;

#ifndef ROUNDS
#define ROUNDS 10
#endif

#ifndef SKIP_ROUNDS
#define SKIP_ROUNDS 5
#endif

size_t NMessages = 0;
int rank;

struct Receiver : public ff::ff_node {
 
    void* svc(void* in){
        double start_time = 0, end_time = 0;
        if (rank == 0){
            MTCL::Manager::listen("MPI:0");
            auto handle = MTCL::Manager::getNext();
            char* buffer_send = (char*)calloc(100*MessageSize, sizeof(char));
            buffer_send[0] = 'A'; buffer_send[MessageSize-1] = 'B';
            size_t sz;

            custom_barrier();
             for(size_t it = 0; it < ROUNDS + SKIP_ROUNDS; ++it){
                if (it == SKIP_ROUNDS) start_time = MPI_Wtime();
                ff_send_out(buffer_send);
                handle.probe(sz);
                char* buffer_receive = (char*)malloc(sz);
                handle.receive(buffer_receive, sz);
                memcpy(buffer_send, buffer_receive, sz);
                free(buffer_receive);
             }
             end_time = MPI_Wtime();
             if (buffer_send[0] != 'A' && buffer_send[MessageSize-1] != 'B') {
                std::cerr << "Message received is corrupted\n";
                abort();
            }
            free(buffer_send);
            handle.close();

            std::cout << "MTCL;" << MessageSize << ";" << (((end_time - start_time)*1000000)/ROUNDS) << std::endl;
        }
        if (rank == 1){
            MTCL::Manager::listen("MPI:1");
            auto handle = MTCL::Manager::getNext();
            custom_barrier();
            size_t mSize;
            for (size_t it = 0;  it < ROUNDS + SKIP_ROUNDS; ++it ){
                handle.probe(mSize);
                char* buffer = (char*)malloc(mSize);
                handle.receive(buffer, mSize);
                ff_send_out(buffer);
            }
            handle.close();
        }

	    return this->EOS;
    }
};

struct Forwarder : public ff::ff_node {
    void* svc(void* in){
	return in;
    }
};

struct Sender : public ff::ff_node {
    MTCL::HandleUser handle;
    int svc_init(){
        if (rank == 0){
            handle = std::move(MTCL::Manager::connect("MPI:1"));
        }
        if (rank == 1){
            handle = std::move(MTCL::Manager::connect("MPI:0"));
        }
        return 0;
    }

    void* svc(void* in){
	((char*)in)[1] = 'B';
        handle.send(in, MessageSize);
        if (rank == 1)
            free(in);
        return GO_ON;
    }

    void svc_end(){
        handle.close();
    }
};

int main(int argc, char*argv[]){

    if (argc < 2){
        std::cout << "Usage: " << argv[0] << " MessageSize"  << std::endl;
        return -1;
    }
    MTCL::Manager::init("Test");
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MessageSize = strtoul(argv[1], nullptr, 0);
    
    Receiver r;
    Sender s;
    Forwarder f;
    ff_pipeline pipe;
    pipe.add_stage(&r);
    pipe.add_stage(&f);
    pipe.add_stage(&s);
    

    /*if (pipe.run_and_wait_end()<0) {
      error("running mainPipe\n");
      return -1;
    }*/
	pipe.run();
	pipe.wait();

    MTCL::Manager::finalize();
    return 0;
}

