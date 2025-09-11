#include <iostream>
#include <fstream>
#include <vector>
#include <mpi.h>


int main(int argc, char** argv) {

    MPI_Init(&argc, &argv);

    MPI_Finalize();

    int size, rank;
MPI_Comm_size(MPI_COMM_WORLD, &size);
MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    return 0;
}