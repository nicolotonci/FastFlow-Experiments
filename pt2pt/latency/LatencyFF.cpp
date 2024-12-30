#include <ff/dff.hpp>
#include <iostream>
#include <mpi.h>
#include <mutex>

using namespace ff;

// to test serialization without using Cereal
#define MANUAL_SERIALIZATION 1

// ------------------------------------------------------
size_t NMessages = 0;
size_t MessageSize = 0;


int send_val = 1;  // Valore arbitrario per la riduzione
int recv_val = 0;

void inline custom_barrier() {
   // MPI_Allreduce(&send_val, &recv_val, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
}

struct ExcType {
    char* content = nullptr;

	ExcType() { }

    ExcType(size_t s){
        content = (char*)malloc(MessageSize);
    }
	
	
#if !defined(MANUAL_SERIALIZATION)
	template<class Archive>
	void serialize(Archive & archive) {
	  archive(cereal::binary_data(content, MessageSize));
	}
#endif	
		
};


#ifdef MANUAL_SERIALIZATION
template<typename Buffer>
bool serialize(Buffer&b, ExcType* input){
	b = {input->content, MessageSize};
	return false;
}

template<typename T>
void serializefreetask(T *o, ExcType* input) {
    free(input->content);
	delete input;
}

template<typename Buffer>
bool deserialize(const Buffer&b, ExcType* p){
	p->content = b.first;
	return false;
}
#endif


struct Producer : ff::ff_monode_t<ExcType>{
    size_t items;
    size_t dataLength;
    double start_time, end_time;
    bool firstRound = true;
    Producer(size_t itemsToGenerate, size_t dataLength): items(itemsToGenerate), dataLength(dataLength) {}

    ExcType* svc(ExcType* in){
        if (!in){
            return new ExcType(MessageSize);
        }
        if (firstRound){
            firstRound = false;
            start_time = MPI_Wtime();
            return in;
        }
        end_time = MPI_Wtime();  

#ifdef MANUAL_SERIALIZATION
        free(in->content);
	    delete in;
#else	  
	    delete in;
#endif	

	    return this->EOS;
    }

    void svc_end(){
        std::cout << "Round trip time of a message of size (" 
                  << MessageSize << " bytes) = " << ((end_time - start_time)*1000000) << " us.\n";
    }	
};

struct Consumer : ff::ff_minode_t<ExcType>{
    int svc_init(){
        custom_barrier();
        return 0;
    }

    ExcType* svc(ExcType* in){
        ff::cout << "Recevied something forwording it now!\n";
        return in;
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
    
    Producer p(NMessages, MessageSize);
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
