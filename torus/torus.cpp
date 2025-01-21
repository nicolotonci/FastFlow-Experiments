#include <iostream> // cerr
#include <ff/dff.hpp>

#define BASE_TYPE_PAYLOAD
#define MANUAL_SERIALIZATION
#include "../utils/payload.hpp"


using namespace ff;

// some globals
const long NUMTASKS=1048576;
const long bigbatchSize=1024;
const long smallbatchSize=256; // 64
unsigned int nticks=0;
long numBatch;


unsigned int nstages = 7;

struct masterStage: public ff_monode_t<ExcType> {
    int svc_init() { eossent=false; return 0;}
    ExcType * svc(ExcType * task) {
        if (task==NULL) {
            gettimeofday(&tstart,NULL);
            for(long i=0;i<bigbatchSize;++i)
                if (!ff_send_out_to(new ExcType(i), 0)) abort();
            //if (numBatch>0) --numBatch;
            return FLUSH;
        }
        
        if (numBatch) {
            for(long i=0;i<smallbatchSize;++i)
                if (!ff_send_out_to(new ExcType(i), 0)) abort();
            --numBatch;
        } else if (!eossent) {
            ff_send_out_to(EOS, 0);
            eossent=true;
        }
        ticks_wait(nticks);
        return FLUSH;
    };
    void svc_end(){
        gettimeofday(&tend,NULL);
        auto time = diffmsec(tend, tstart);
        printf("%d;%ld;%g;%f\n", nstages, NUMTASKS, time,(NUMTASKS*nstages*1000.0)/time);
    }
    bool eossent;
    struct timeval tstart, tend;
};

// all other stages
struct Stage: public ff_node_t<ExcType> {
    ExcType * svc(ExcType * task) {  ticks_wait(nticks); return task; }
};

int main(int argc, char * argv[]) {
    DFF_Init(argc, argv);

    nticks = 1000;
    if (argc>1) {
        if (argc!=3) {
            std::cerr << "usage: " << argv[0] << " num-stages nticks\n";
            return -1;
        }
        
        nstages  = atoi(argv[1]);
        nticks   = atoi(argv[2]);
    }

    numBatch=((NUMTASKS-bigbatchSize)/smallbatchSize);
    
    if (nstages<2) {
        std::cerr << "invalid number of stages\n";
        return -1;
    }
    
    ff_pipeline pipe(false,512,512,true);
    masterStage master;
    pipe.add_stage(&master);
    pipe.createGroup("G0") << &master;
    

    for(unsigned i=1;i<nstages;++i){
        auto* stage = new Stage;
        pipe.add_stage(stage);
        pipe.createGroup("G"+std::to_string(i)) << stage;
    }

    pipe.wrap_around();
    
    if (pipe.run_and_wait_end()<0) {
        error("running pipeline\n");
        return -1;
    }

    return 0;
}
