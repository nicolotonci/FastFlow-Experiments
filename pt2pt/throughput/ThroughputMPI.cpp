#include <iostream>
#include <mpi.h>
int send_val = 1;  // Valore arbitrario per la riduzione
int recv_val = 0;

void inline custom_barrier() {
    MPI_Allreduce(&send_val, &recv_val, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
}


int main(int argc, char** argv){

    if(argc < 3) {
        printf("Usage: %s <messages> <messageSize>\n", argv[0]);
        return 1;
    }
    MPI_Init(&argc, &argv);
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

    // Listening for new connections, expecting "ping", sending "pong"
    if(rank == 0) {
        // get the handle 

        //MPI_Barrier(MPI_COMM_WORLD);
        custom_barrier();
        start_time = MPI_Wtime(); // Inizia il timer

        for (size_t i = 0; i < NMessages; ++i) {
            char* buffer = (char*)calloc(MessageSize, sizeof(char));
            MPI_Send(buffer, MessageSize, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
            free(buffer);
        }
        MPI_Send("E", 1, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
        custom_barrier();
        //MPI_Barrier(MPI_COMM_WORLD); // Sincronizzazione finale
        end_time = MPI_Wtime(); // Ferma il timer
        std::cout << "Producer: Total time for sending " << NMessages << " messages of size " 
                  << MessageSize << " = " << ((end_time - start_time)*1000) << " ms.\n";

    }
    else {
        //MPI_Barrier(MPI_COMM_WORLD); // Sincronizzazione iniziale
        custom_barrier();
        start_time = MPI_Wtime(); // Inizia il timer
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
        custom_barrier();
        //MPI_Barrier(MPI_COMM_WORLD);
        end_time = MPI_Wtime(); 

        std::cout << "Consumer: Total time for receiving " << NMessages << " messages of size " 
                  << MessageSize << " = " << ((end_time - start_time)*1000) << " ms.\n";
    }

    return MPI_Finalize();
}
