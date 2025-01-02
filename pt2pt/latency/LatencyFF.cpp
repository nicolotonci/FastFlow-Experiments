#include <ff/dff.hpp>
#include <iostream>
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


struct Producer : ff::ff_monode_t<ExcType>{
    double start_time, end_time;
    int rounds = 0;

    ExcType* svc(ExcType* in){
        if (!in){
            custom_barrier();
            ff_send_out_to(new ExcType(MessageSize), 0);
            return GO_ON;
        }
        if (++rounds < ROUNDS+SKIP_ROUNDS){
            if (rounds == SKIP_ROUNDS)
                start_time = MPI_Wtime();
            ff_send_out_to(in, 0);
            return GO_ON;
        }
        end_time = MPI_Wtime();  

#ifdef DEBUG
        if (in->content[0] != 'C' || in->content[MessageSize-1] != 'O')
            std::cerr << "Message was corrupted!\n";
#endif

	    delete in;

	    return this->EOS;
    }

    void svc_end(){
        std::cout << "Round trip time of a message of size (" 
                  << MessageSize << " bytes) = " << (((end_time - start_time)*1000000)/ROUNDS) << " us.\n";
    }	
};

struct Consumer : ff::ff_monode_t<ExcType>{
    int svc_init(){
        custom_barrier();
        return 0;
    }

    ExcType* svc(ExcType* in){
        ff_send_out_to(in, 0);
        return GO_ON;
    }
};

int main(int argc, char*argv[]){
    
    if (DFF_Init(argc, argv) != 0) {
		error("DFF_Init\n");
		return -1;
	}

    if (argc < 2){
        std::cout << "Usage: " << argv[0] << " MessageSize"  << std::endl;
        return -1;
    }

    MessageSize = strtoul(argv[1], nullptr, 0);
    
    Producer p;
    Consumer c;
    ff_pipeline pipe;
    pipe.add_stage(&p);
    pipe.add_stage(&c);
    pipe.wrap_around();
    
    createGroup("G0") << &p;
    createGroup("G1") << &c;

    if (pipe.run_and_wait_end()<0) {
      error("running mainPipe\n");
      return -1;
    }	
    return 0;
}
