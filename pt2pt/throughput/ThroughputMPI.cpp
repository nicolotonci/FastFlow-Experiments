#include <iostream>
#include <mpi.h>
#include "../../utils/threadMapping.hpp"
#include "../../utils/synchronization.hpp"

int main(int argc, char** argv){

    if(argc < 3) {
        printf("Usage: %s <messages> <messageSize> [threadID]\n", argv[0]);
        return 1;
    }

    //if provided, set the thread mapping
    if (argc == 4) pinThreadToCore(atoi(argv[3]));
    
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

    size_t NMessages = strtoul(argv[1], nullptr, 0);
    size_t MessageSize = strtoul(argv[2], nullptr, 0);
    
    double start_time, end_time;

    if(rank == 0) {
        char* buffer = (char*)calloc(MessageSize, sizeof(char));
        custom_barrier();
        start_time = MPI_Wtime();

        for (size_t i = 0; i < NMessages; ++i) 
            MPI_Send(buffer, MessageSize, MPI_CHAR, 1, 0, MPI_COMM_WORLD);

        MPI_Send("E", 1, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
        end_time = MPI_Wtime();
        free(buffer);
        //std::cout << "Producer: Total time for sending " << NMessages << " messages of size " 
        //          << MessageSize << " = " << ((end_time - start_time)*1000) << " ms.\n";

    }
    else {
        custom_barrier();
        start_time = MPI_Wtime();
        int mSize;
        MPI_Status s;
        MPI_Probe(0, 0, MPI_COMM_WORLD, &s);
        MPI_Get_count(&s, MPI_CHAR, &mSize);
        while(mSize > 1){
            char* buffer = (char*)malloc(mSize);
            MPI_Recv(buffer, mSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            free(buffer);
            MPI_Probe(0, 0, MPI_COMM_WORLD, &s);
            MPI_Get_count(&s, MPI_CHAR, &mSize);
        }
        end_time = MPI_Wtime(); 

        //std::cout << "Consumer: Total time for receiving " << NMessages << " messages of size " 
        //          << MessageSize << " = " << ((end_time - start_time)*1000) << " ms.\n";
        std::cout << "MPI;" << NMessages << ";" << MessageSize << ";" << ((end_time - start_time)*1000) << std::endl;

    }

    return MPI_Finalize();
}
