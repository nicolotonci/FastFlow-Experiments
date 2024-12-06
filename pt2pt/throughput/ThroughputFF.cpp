#include <ff/dff.hpp>
#include <iostream>
#include <mpi.h>
#include <mutex>

using namespace ff;

// to test serialization without using Cereal
//#define MANUAL_SERIALIZATION 1

// ------------------------------------------------------
size_t NMessages = 0;
size_t MessageSize = 0;


int send_val = 1;  // Valore arbitrario per la riduzione
int recv_val = 0;

void inline custom_barrier() {
    MPI_Allreduce(&send_val, &recv_val, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
}

struct ExcType {
	ExcType():contiguous(false) {}
	ExcType(bool): contiguous(true) {}
	~ExcType() {
		if (!contiguous)
			delete [] C;
	}
	
	size_t clen = 0;
	char*  C    = nullptr;
	bool contiguous;
	
#if !defined(MANUAL_SERIALIZATION)
	template<class Archive>
	void serialize(Archive & archive) {
	  archive(clen);
	  if (!C) 
		  C =new char[clen];
	  archive(cereal::binary_data(C, clen));
	}
#endif	
		
};


#ifdef MANUAL_SERIALIZATION
template<typename Buffer>
bool serialize(Buffer&b, ExcType* input){
	b = {(char*)input, input->clen+sizeof(ExcType)};

	return false;
}

template<typename T>
void serializefreetask(T *o, ExcType* input) {
	input->~ExcType();
	free(o);
}

template<typename Buffer>
void deserializealloctask(const Buffer& b, ExcType*& p) {
	p = new (b.first) ExcType(true);

};

template<typename Buffer>
bool deserialize(const Buffer&b, ExcType* p){
	p->clen = b.second - sizeof(ExcType);
	p->C = (char*)p + sizeof(ExcType);

	return false;
}
#endif

static inline ExcType* allocateExcType(size_t size, bool setdata=false) {
	char* _p = (char*)calloc(size+sizeof(ExcType), 1);	// to make valgrind happy !
	ExcType* p = new (_p) ExcType(true);  // contiguous allocation
	
	p->clen    = size;
	p->C       = (char*)p+sizeof(ExcType);
	/*if (setdata) {
		p->C[0]       = 'c';
		if (size>10) 
			p->C[10]  = 'i';
		if (size>100)
			p->C[100] = 'a';
		if (size>500)
			p->C[500] = 'o';		
	}
	p->C[p->clen-1] = 'F';*/
	return p;
}


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
       for(size_t i=0; i< items; i++){
		   ff_send_out(allocateExcType(dataLength, false));
       }        
	   return this->EOS;
    }

    void svc_end(){
        custom_barrier();
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
	  
#ifdef MANUAL_SERIALIZATION
	  in->~ExcType(); free(in);
#else	  
	  delete in;
#endif	  
      return this->GO_ON;
    }

    void svc_end(){
        custom_barrier();
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
