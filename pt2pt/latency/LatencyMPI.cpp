#include <iostream>
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

    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
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
    
    double start_time = 0, end_time = 0;

    if(rank == 0) {
        
        char* buffer = (char*)calloc(MessageSize, sizeof(char));
        buffer[0] = 'A'; buffer[MessageSize-1] = 'B';
        custom_barrier();
        for(size_t it = 0; it < ROUNDS + SKIP_ROUNDS; ++it){
            if (it == SKIP_ROUNDS) start_time = MPI_Wtime();
            MPI_Send(buffer, MessageSize, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
            MPI_Recv(buffer, MessageSize, MPI_CHAR, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        end_time = MPI_Wtime();

        if (buffer[0] != 'A' && buffer[MessageSize-1] != 'B') {
            std::cerr << "Message received is corrupted\n";
            abort();
        }

        free(buffer);
                
        std::cout << "Round trip time of a message of size (" 
                  << MessageSize << " bytes) = " << (((end_time - start_time)*1000000)/ROUNDS) << " us.\n";

    }
    else {
        int mSize;
        MPI_Status s;
        custom_barrier();
        for (size_t it = 0;  it < ROUNDS + SKIP_ROUNDS; ++it ){
            MPI_Probe(0, 0, MPI_COMM_WORLD, &s);
            MPI_Get_count(&s, MPI_CHAR, &mSize);
            
            char* buffer = (char*)malloc(mSize);
            MPI_Recv(buffer, mSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Send(buffer, mSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
            free(buffer);
        }
    }

    return MPI_Finalize();
}
