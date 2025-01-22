#include <ff/dff.hpp>
#include <iostream>
#include <mpi.h>
#include <mutex>

#define MANUAL_SERIALIZATION
#include "../utils/payload.hpp"

using namespace ff;

#ifndef ROUNDS
#define ROUNDS 10
#endif

#ifndef SKIP_ROUNDS
#define SKIP_ROUNDS 5
#endif

size_t NMessages = 0;


struct Producer : ff::ff_monode_t<ExcType>{
    unsigned long start_time, end_time;
    const std::string experimentStr;
    int rounds = 0;
    Producer(const std::string experiment) : experimentStr(experiment) {}
    ExcType* svc(ExcType* in){
        if (!in){
            ff_send_out_to(new ExcType(MessageSize), 0);
            return GO_ON;
        }
        if (++rounds < ROUNDS+SKIP_ROUNDS){
            if (rounds == SKIP_ROUNDS)
                start_time = ff::getusec();
            ff_send_out_to(in, 0);
            return GO_ON;
        }
        end_time = ff::getusec();  

#ifdef DEBUG
        if (in->content[0] != 'C' || in->content[MessageSize-1] != 'O')
            std::cerr << "Message was corrupted!\n";
#endif

	    delete in;

	    return this->EOS;
    }

    void svc_end(){
        std::cout << experimentStr << ";" << MessageSize << ";" << (((end_time - start_time)*1000)/ROUNDS) << std::endl;

        //std::cout << "Round trip time of a message of size (" 
        //          << MessageSize << " bytes) = " << (((end_time - start_time)*1000000)/ROUNDS) << " us.\n";
    }	
};

struct Consumer : ff::ff_monode_t<ExcType>{
    int svc_init(){
        return 0;
    }

    ExcType* svc(ExcType* in){
        ff_send_out_to(in, 0);
        return GO_ON;
    }
};

struct dummyWrapper: public internal_mi_transformer {
    
    dummyWrapper(ff_node* n) : internal_mi_transformer(this, false) {
        this->n = n;
        internal_mi_transformer::registerCallback(ff_send_out_to_cbk, this);
    }

    bool serialize(void* task, int id){
        ff_minode::ff_send_out(task); 
        return true;
    }

    static inline bool ff_send_out_to_cbk(void* task, int id,
										  unsigned long retry,
										  unsigned long ticks, void* obj) {		
		return ((dummyWrapper*)obj)->serialize(task, id);
	}

};

int main(int argc, char*argv[]){

    if (argc < 2){
        std::cout << "Usage: " << argv[0] << " MessageSize"  << std::endl;
        return -1;
    }

    MessageSize = strtoul(argv[1], nullptr, 0);
    MessageAllocator::init(100000);

    {  /// PLAIN EXAMPLE
        Producer p("PLAIN");
        Consumer c;
        ff_pipeline pipe;
        pipe.add_stage(&p);
        pipe.add_stage(&c);
        pipe.wrap_around();
        
        pipe.run();
        pipe.wait();
    }
    {
        Producer p("TRASFORMER");
        Consumer c; 
        ff_pipeline pipe;
        pipe.add_stage(new dummyWrapper(&p));
        pipe.add_stage(new dummyWrapper(&c));
        pipe.wrap_around();

        pipe.run();
        pipe.wait();
    }
    {
        Producer p("WRAPPER"); p.mioID = 1;
        Consumer c; p.mioID = 2; 
        std::pair<ff::IngressChannels_t, ff::EgressChannels_t> f = std::make_pair<ff::IngressChannels_t, ff::EgressChannels_t>({std::make_tuple(&c, ChannelType::FBK, ChannelLocality::REMOTE, -1)}, {std::make_tuple(&c, ChannelType::FWD, ChannelLocality::REMOTE, -1)});
        std::pair<ff::IngressChannels_t, ff::EgressChannels_t> s = std::make_pair<ff::IngressChannels_t, ff::EgressChannels_t>({std::make_tuple(&p, ChannelType::FWD, ChannelLocality::REMOTE, -1)}, {std::make_tuple(&p, ChannelType::FBK, ChannelLocality::REMOTE, -1)});
        ff_pipeline pipe;
        pipe.add_stage(new WrapperINOUT(&p, f, false));
        pipe.add_stage(new WrapperINOUT(&c, s, false));
        pipe.wrap_around();
        
        pipe.run();
        pipe.wait();
    }
    return 0;
}
