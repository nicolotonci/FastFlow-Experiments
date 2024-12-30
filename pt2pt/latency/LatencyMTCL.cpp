#include <iostream>
#define SINGLE_IO_THREAD
#include "mtcl.hpp"
#include <mpi.h>

int send_val = 1; 
int recv_val = 0;

void inline custom_barrier() {
    MPI_Allreduce(&send_val, &recv_val, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
}


int main(int argc, char** argv){

    if(argc < 2) {
        printf("Usage: %s <messageSize>\n", argv[0]);
        return 1;
    }
    
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
    double start_time, end_time;

    // Listening for new connections, expecting "ping", sending "pong"
    if(rank == 0) {
        MTCL::Manager::listen("MPI:0");
        // get the handle 
        auto handle = MTCL::Manager::getNext();
        char* buffer = (char*)calloc(MessageSize, sizeof(char));
        buffer[0] = 'A'; buffer[MessageSize-1] = 'B';

        custom_barrier();

        start_time = MPI_Wtime(); // Inizia il timer
        handle.send(buffer, MessageSize);
        handle.receive(buffer, MessageSize);
        end_time = MPI_Wtime(); // Ferma il timer

        if (buffer[0] != 'A' && buffer[MessageSize-1] != 'B') {
            std::cerr << "Message received is corrupted\n";
            abort();
        }

        free(buffer);
        handle.close();

        std::cout << "Round trip time of a message of size (" 
                  << MessageSize << " bytes) = " << ((end_time - start_time)*1000000) << " us.\n";

    }
    else {
        size_t mSize;
        auto handle = MTCL::Manager::connect("MPI:0");
        custom_barrier(); 
        handle.probe(mSize);
        char* buffer = (char*)malloc(mSize);
        handle.receive(buffer, mSize);
        handle.send(buffer, mSize);
        free(buffer);
    
        handle.close();
    }

    MTCL::Manager::finalize(true);

    return 0;
}
