#include <ff/dff.hpp>
#include <iostream>
#include <mpi.h>
#include <mutex>
#include "../../utils/synchronization.hpp"

using namespace ff;

size_t NMessages = 0;
size_t MessageSize = 0;

struct ExcType {
    std::map<int32_t, int32_t> myMap;

    ExcType(size_t contentSize){
        for(size_t i = 0; i < contentSize; i++) myMap[(int32_t)i] = (int32_t)i;
    }

    ExcType() = default;

    static void freeTask(ExcType* ptr){
#ifdef REUSE_PAYLOAD 
        return;
#else
        delete ptr;
#endif
    }

#ifndef MANUAL_SERIALIZATION
    template<class Archive>
    void serialize(Archive & archive) {
        archive(myMap); 
    }
#else
    std::tuple<char*, size_t, bool> serialize(){
        size_t count = myMap.size();
        outSize = sizeof(size_t) + count * 2 * sizeof(int32_t);
        char* buffer = (char*) malloc(outSize);

        // Write count
        std::memcpy(buffer, &count, sizeof(size_t));

        // Write key-value pairs
        int32_t* ptr = reinterpret_cast<int32_t*>(buffer + sizeof(size_t));
        for (const auto& [key, value] : myMap) {
            *ptr++ = key;
            *ptr++ = value;
        }

        return {buffer, outSize, true};
    }

    bool deserialize(char* buffer, size_t sz){
    
        size_t count;
        std::memcpy(&count, buffer, sizeof(size_t));

        size_t expectedSize = sizeof(uint32_t) + count * 2 * sizeof(size_t);
        if (sz < expectedSize) {
            std::cerr << "Invalid buffer!\n";
            abort();
        }

        const int32_t* ptr = reinterpret_cast<const int32_t*>(buffer + sizeof(size_t));
        for (uint32_t i = 0; i < count; ++i) {
            int32_t key = *ptr++;
            int32_t value = *ptr++;
            myMap[key] = value;
        }

        return true;
    }
#endif
};

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
        //std::cout << "Producer: Total time for sending " << NMessages << " messages of size " 
        //          << MessageSize << " = " << ((end_time - start_time)*1000) << " ms.\n"; 
    }	
};

struct Consumer : ff::ff_minode_t<ExcType>{
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
        // We print MessageSize*8 since each k,v pair are 8 byte in memory
        std::cout << "FF;" << NMessages << ";" << MessageSize*8 << ";" << ((end_time - start_time)*1000) << std::endl;
        //std::cout << "Consumer: Total time for receiving " << NMessages << " messages of size " 
        //          << MessageSize << " = " << ((end_time - start_time)*1000) << " ms.\n";
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
