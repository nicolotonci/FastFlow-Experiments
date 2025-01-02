#ifndef SYNC_HPP
#define SYNC_HPP
#include <mpi.h>

// global values for syncronization
int send_val = 1;
int recv_val = 0;

void inline custom_barrier() {
    MPI_Allreduce(&send_val, &recv_val, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
}


#endif