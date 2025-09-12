#include <iostream>
#include <fstream>
#include <vector>
#include <mpi.h>


int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

    int size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);  // número de procesos (p)
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);  // id del proceso (r)

    // Queremos leer las dimensiones del archivo

    int nrows = 0, ncols = 0;
    if (rank == 0)
    {
        std::ifstream file("matrix.txt");
        if (!file.is_open()) {
            std::cerr << "Unable to open file.'\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        file >> nrows >> ncols;  // 
        file.close();
    }

    // Usamos Broadcast para pasar estos datos a todos los procesos
    MPI_Bcast(&nrows, 1, MPI_INT, 0, MPI_COMM_WORLD); // Distribuye filas
    MPI_Bcast(&ncols, 1, MPI_INT, 0, MPI_COMM_WORLD); // Distribuye columnas

    std::cout << "Tamaño de la matriz" << std::endl;
    std::cout << "Filas: " << nrows << ", Columnas: " << ncols << std::endl;

    

    MPI_Finalize();

    return 0;
}