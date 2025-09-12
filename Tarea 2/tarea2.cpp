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

    // Imprimimos el tamaño de cada proceso
    std::cout << "Tamaño de la matriz" << std::endl;
    std::cout << "Filas: " << nrows << ", Columnas: " << ncols << std::endl;

    // Calculamos el tamaño de cada bloque de la matriz en cada proceso

    const int n = nrows;  // Definimos n como el número de filas
    const int p = size;  // p siendo el num de procesos
    const int r = rank;  // r siendo el id del proceso

    const int q   = n / p;   // filas base por proceso
    const int rem = n % p;   // procesos que reciben una fila extra

    const int my_nrows = (r < rem) ? (q + 1) : q;  // 

    int my_firstrow;
    if (r < rem) {
        my_firstrow = r * (q + 1);
    } else {
        my_firstrow = rem * (q + 1) + (r - rem) * q;
    }
    const int my_lastrow = (my_nrows > 0) ? (my_firstrow + my_nrows - 1)
                                          : (my_firstrow - 1);


    MPI_Finalize();

    return 0;
}