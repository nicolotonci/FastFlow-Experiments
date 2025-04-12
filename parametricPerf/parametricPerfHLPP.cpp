/* 
 * FastFlow concurrent network:
 * 
 *           |--> MiNode 
 *  MoNode-->|           
 *           |--> MiNode 
 *  MoNode-->|           
 *           |--> MiNode 
 *
 * /<------- a2a ------>/
 *
 * distributed group names: 
 *  S*: all left-hand side nodes
 *  D*: all righ-hand side nodes
 *
 */

// running the tests with limited buffer capacity
#define FF_BOUNDED_BUFFER
#define DEFAULT_BUFFER_CAPACITY 512


#include <ff/dff.hpp>
#include <iostream>
#include <mutex>
#include <chrono>

using namespace ff;

// to test serialization without using Cereal
#define MANUAL_SERIALIZATION
#include "../utils/payload.hpp"

#include "../utils/delays.hpp"
#include "../utils/synchronization.hpp"

std::mutex mtx;  // used only for pretty printing

struct MoNode : ff::ff_monode_t<ExcType>{
    int items, execTime;
    MoNode(int itemsToGenerate, int execTime):
		items(itemsToGenerate), execTime(execTime){}

    ExcType* svc(ExcType*){
       for(int i=0; i< items; i++){
		   if (execTime) active_delay(this->execTime);
		   ff_send_out(new ExcType(MessageSize));
       }        
	   return this->EOS;
    }

    void svc_end(){
#ifdef DEBUG
        const std::lock_guard<std::mutex> lock(mtx);
        ff::cout << "[MoNode" << this->get_my_id() << "] Generated Items: " << items << ff::endl;
#endif
    }	
};

struct MiNode : ff::ff_minode_t<ExcType>{
    int processedItems = 0;
    int execTime;
    MiNode(int execTime): execTime(execTime) {}

    ExcType* svc(ExcType* in){
	if (execTime) active_delay(this->execTime);

      ++processedItems;

#ifdef BASE_TYPE_PAYLOAD
	  free(in);
#else	  
	  delete in;
#endif	 
      return this->GO_ON;
    }

    void svc_end(){
#ifdef DEBUG
        const std::lock_guard<std::mutex> lock(mtx);
        ff::cout << "[MiNode" << this->get_my_id() << "] Processed Items: " << processedItems << ff::endl;
#endif
    }
};

int main(int argc, char*argv[]){
    
    if (DFF_Init(argc, argv) != 0) {
		error("DFF_Init\n");
		return -1;
	}

    if (argc < 7){
        std::cout << "Usage: " << argv[0] << " #items #byteXitem #execTimeSource #execTimeSink #np #nwXp [ondemand Queue Length]"  << std::endl;
        return -1;
    }
    int items = atoi(argv[1]);
    MessageSize = atol(argv[2]);
    int execTimeSource = atoi(argv[3]);
    int execTimeSink = atoi(argv[4]);
    int numProc = atoi(argv[5]);
	int numWorkerXProcess = atoi(argv[6]);
	int ondemandLength = 0;
	if (argc == 8) ondemandLength = atoi(argv[7]);
#ifdef DEBUG
	ff::cout << "Ondemand set to: " << ondemandLength << std::endl;
#endif
    ff_a2a a2a;

    std::vector<MoNode*> sxWorkers;
    std::vector<MiNode*> dxWorkers;


    sxWorkers.push_back(new MoNode(items, execTimeSource));

    for(int i = 0; i < (numProc*numWorkerXProcess); i++)
        dxWorkers.push_back(new MiNode(execTimeSink));

    a2a.add_firstset(sxWorkers, ondemandLength, true);
    a2a.add_secondset(dxWorkers, true);


	for(int i = 0; i < numProc; i++){
		auto g = a2a.createGroup(std::string("G")+std::to_string(i));
        
        if (i == 0) g << sxWorkers.back(); // add the emitter

		for(int j = i*numWorkerXProcess; j < (i+1)*numWorkerXProcess; j++){
			g << dxWorkers[j];	
		}
	}
	// synchronization point
	custom_barrier();

	auto t0=getusec();
    if (a2a.run_and_wait_end()<0) {
      error("running mainPipe\n");
      return -1;
    }
    std::cout << DFF_getMyGroup() << " terminated!\n";
	custom_barrier();
	auto t1=getusec();

#ifndef DISABLE_FF_DISTRIBUTED
	if (DFF_getMyGroup() == "G0")
#endif
		std::cout << items << ";" << MessageSize << ";" << numProc << ";" << numWorkerXProcess << ";" << ondemandLength << ";" << execTimeSource << ";" << execTimeSink << ";" << (t1-t0)/1000.0 << "\n";
    std::cout << "All temrinated!\n";
    return 0;
}
