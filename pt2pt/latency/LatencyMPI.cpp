#include <iostream>
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

    size_t MessageSize = strtoul(argv[1], nullptr, 0);
    
    // timers variables
    double start_time, end_time;

    // Listening for new connections, expecting "ping", sending "pong"
    if(rank == 0) {
        // get the handle 

        //MPI_Barrier(MPI_COMM_WORLD);
        
        char* buffer = (char*)calloc(MessageSize, sizeof(char));
        buffer[0] = 'A'; buffer[MessageSize-1] = 'B';
        custom_barrier();
        start_time = MPI_Wtime(); // Inizia il timer
        MPI_Send(buffer, MessageSize, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
        
        MPI_Recv(buffer, MessageSize, MPI_CHAR, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        end_time = MPI_Wtime(); // Ferma il timer

        if (buffer[0] != 'A' && buffer[MessageSize-1] != 'B') {
            std::cerr << "Message received is corrupted\n";
            abort();
        }

        free(buffer);
                
        std::cout << "Round trip time of a message of size (" 
                  << MessageSize << " bytes) = " << ((end_time - start_time)*1000000) << " us.\n";

    }
    else {
        int mSize;
        MPI_Status s;
        custom_barrier();
        MPI_Probe(0, 0, MPI_COMM_WORLD, &s);
        MPI_Get_count(&s, MPI_CHAR, &mSize);
        
        char* buffer = (char*)malloc(mSize);
        MPI_Recv(buffer, mSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Send(buffer, mSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        free(buffer);
    }

    return MPI_Finalize();
}
