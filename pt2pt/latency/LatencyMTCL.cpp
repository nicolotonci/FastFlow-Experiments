#include <iostream>
#define MTCL_DISABLE_COLLECTIVES
#define SINGLE_IO_THREAD
#define NO_MTCL_MULTITHREADED
#include "mtcl.hpp"
#include <mpi.h>
#include "../../utils/threadMapping.hpp"
#include "../../utils/synchronization.hpp"

#ifndef ROUNDS
#define ROUNDS 10
#endif

#ifndef SKIP_ROUNDS
#define SKIP_ROUNDS 5
#endif

int main(int argc, char** argv){

    if(argc < 2) {
        printf("Usage: %s <messageSize> [threadID]\n", argv[0]);
        return 1;
    }

    //if provided, set the thread mapping
    if (argc == 3) pinThreadToCore(atoi(argv[2]));
    
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

    size_t MessageSize = strtoul(argv[1], nullptr, 0);
    
    // timers variables
    double start_time = 0, end_time = 0;

    if(rank == 0) {
        MTCL::Manager::listen("MPI:0");
        auto handle = MTCL::Manager::getNext();
        char* buffer = (char*)calloc(MessageSize, sizeof(char));
        buffer[0] = 'A'; buffer[MessageSize-1] = 'B';

        custom_barrier();
        for(size_t it = 0; it < ROUNDS + SKIP_ROUNDS; ++it){
            if (it == SKIP_ROUNDS) start_time = MPI_Wtime();
            handle.send(buffer, MessageSize);
            handle.receive(buffer, MessageSize);
        }

        end_time = MPI_Wtime();

        if (buffer[0] != 'A' && buffer[MessageSize-1] != 'B') {
            std::cerr << "Message received is corrupted\n";
            abort();
        }

        free(buffer);
        handle.close();

        std::cout << "Round trip time of a message of size (" 
                  << MessageSize << " bytes) = " << (((end_time - start_time)*1000000)/ROUNDS) << " us.\n";

    }
    else {
        size_t mSize;
        auto handle = MTCL::Manager::connect("MPI:0");
        custom_barrier();
        for (size_t it = 0;  it < ROUNDS + SKIP_ROUNDS; ++it ){
            handle.probe(mSize);
            char* buffer = (char*)malloc(mSize);
            handle.receive(buffer, mSize);
            handle.send(buffer, mSize);
            free(buffer);
        }
        handle.close();
    }

    MTCL::Manager::finalize(true);

    return 0;
}
