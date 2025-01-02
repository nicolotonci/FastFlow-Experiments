#include <iostream>
#define SINGLE_IO_THREAD
#include "mtcl.hpp"
#include <mpi.h>
#include "../../utils/threadMapping.hpp"
#include "../../utils/synchronization.hpp"

int main(int argc, char** argv){

    if(argc < 3) {
        printf("Usage: %s <messages> <messageSize> [threadID]\n", argv[0]);
        return 1;
    }

    //if provided, set the thread mapping
    if (argc == 3) pinThreadToCore(atoi(argv[3]));

    
	MTCL::Manager::init("ThroughputTest");
    
    int rank, size;
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != 2) {
        if (rank == 0)
            std::cerr << "This program requires exactly 2 processes.\n";
        MPI_Finalize();
        return -1;
    }

    size_t NMessages = strtoul(argv[1], nullptr, 0);
    size_t MessageSize = strtoul(argv[2], nullptr, 0);
    
    // timers variables
    double start_time, end_time;

    if(rank == 0) {
        MTCL::Manager::listen("MPI:0");
        auto handle = MTCL::Manager::getNext();

        custom_barrier();
        start_time = MPI_Wtime();

        for (size_t i = 0; i < NMessages; ++i) {
            char* buffer = (char*)calloc(MessageSize, sizeof(char));
            handle.send(buffer, MessageSize);
            free(buffer);
        }
        handle.close();
        end_time = MPI_Wtime(); 
        std::cout << "Producer: Total time for sending " << NMessages << " messages of size " 
                  << MessageSize << " = " << ((end_time - start_time)*1000) << " ms.\n";

    }
    else {
        auto handle = MTCL::Manager::connect("MPI:0");
        custom_barrier();
        start_time = MPI_Wtime();
        size_t mSize;
        handle.probe(mSize);
        while(mSize){
            char* buffer = (char*)malloc(mSize);
            handle.receive(buffer, mSize);
            free(buffer);
            handle.yield();
            handle = MTCL::Manager::getNext();
            handle.probe(mSize);
        }
        end_time = MPI_Wtime(); // Ferma il timer
        handle.close();

        std::cout << "Consumer: Total time for receiving " << NMessages << " messages of size " 
                  << MessageSize << " = " << ((end_time - start_time)*1000) << " ms.\n";
    }

    MTCL::Manager::finalize(true);

    return 0;
}
