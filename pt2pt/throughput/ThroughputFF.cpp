#include <ff/dff.hpp>
#include <iostream>
#include <mpi.h>
#include <mutex>
#include "../../utils/synchronization.hpp"

#define MANUAL_SERIALIZATION
#define REUSE_PAYLOAD
#define BASE_TYPE_PAYLOAD
#include "../../utils/payload.hpp"


using namespace ff;

size_t NMessages = 0;


struct Producer : ff::ff_monode_t<ExcType>{
    size_t items;
    size_t dataLength;
    double start_time, end_time;
    Producer(size_t itemsToGenerate, size_t dataLength): items(itemsToGenerate), dataLength(dataLength) {}
    
    int svc_init(){
        
        custom_barrier();
        start_time = MPI_Wtime();
        return 0;
    }

    ExcType* svc(ExcType*){
#ifdef REUSE_PAYLOAD
       ExcType* payload = new ExcType(MessageSize);
       for(size_t i=0; i< items; i++){
		   ff_send_out_to(payload, 0);
       } 
#else
        for(size_t i=0; i< items; i++)
		   ff_send_out_to(new ExcType(MessageSize), 0);
#endif
	   return this->EOS;
    }

    void svc_end(){
        end_time = MPI_Wtime();
        std::cout << "Producer: Total time for sending " << NMessages << " messages of size " 
                  << MessageSize << " = " << ((end_time - start_time)*1000) << " ms.\n"; 
    }	
};

struct Consumer : ff::ff_minode_t<ExcType>{
    int processedItems = 0;
    double start_time, end_time;
    int svc_init(){
        custom_barrier();
        start_time = MPI_Wtime();
        return 0;
    }

    ExcType* svc(ExcType* in){
	  
#ifdef BASE_TYPE_PAYLOAD
	  free(in);
#else	  
	  delete in;
#endif	  
      return this->GO_ON;
    }

    void svc_end(){
        end_time = MPI_Wtime();
        std::cout << "Consumer: Total time for receiving " << NMessages << " messages of size " 
                  << MessageSize << " = " << ((end_time - start_time)*1000) << " ms.\n";
    }
};

int main(int argc, char*argv[]){
    
    if (DFF_Init(argc, argv) != 0) {
		error("DFF_Init\n");
		return -1;
	}

    if (argc < 3){
        std::cout << "Usage: " << argv[0] << " #items #byteXitem"  << std::endl;
        return -1;
    }

    NMessages = strtoul(argv[1], nullptr, 0);
    MessageSize = strtoul(argv[2], nullptr, 0);
    

    Producer p(NMessages, MessageSize);
    Consumer c;
    ff_pipeline pipe;
    pipe.add_stage(&p);
    pipe.add_stage(&c);
    
    createGroup("G0") << &p;
    createGroup("G1") << &c;

    if (pipe.run_and_wait_end()<0) {
      error("running mainPipe\n");
      return -1;
    }	
    return 0;
}
