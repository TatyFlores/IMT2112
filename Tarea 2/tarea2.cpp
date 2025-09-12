#include <iostream>
#include <fstream>
#include <vector>
#include <mpi.h>
#include <new>
#include <cstdlib>
#include <cmath>
using namespace std;

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
    std::cout << "Filas: " << nrows << ", Columnas: " << ncols << "\n" << std::endl;

    // Calculamos el tamaño de cada bloque de la matriz en cada proceso

    std::vector<int> cant_rows(size), indice_rows(size);
    {
        int q   = nrows / size;   // filas base por proceso (filas/procesos)
        int rem = nrows % size;   // procesos que reciben una fila extra

        for (int i = 0; i < size; ++i)
            cant_rows[i] = (i < rem) ? (q + 1) : q;
            // hay una cantidad rem que tienen una fila extra, por ello es q+1 filas
            // si se cumple la cantidad rem, tiene q filas

        // desde qué índice global empieza cada rank i (0-based)
        indice_rows[0] = 0;
        for (int i = 1; i < size; ++i)
            indice_rows[i] = indice_rows[i-1] + cant_rows[i-1];
    }

    const int my_nrows = (rank < nrows % size) ? ((nrows / size) + 1) : (nrows / size);

    std::cout << "Rank " << rank
            << "\nCantidad de filas: " << cant_rows[rank]
            << "\nInicio primera fila: " << indice_rows[rank]
            << "\nFin última fila: " << cant_rows[rank] + indice_rows[rank] <<"\n\n";

    std::vector<double> b0(static_cast<size_t>(my_nrows), 1.0);

    // Se utilizó apoyo de ChatGPT para considerar los casos donde hubiera errores eventualmente

    const long long my_elems = 1LL * my_nrows * ncols;
    double* A_local = nullptr;
    if (my_elems > 0) {
        A_local = new (nothrow) double[my_elems];
        if (!A_local) {
            std::cerr << "[rank " << rank << "] Error al reservar memoria (" << my_elems << " doubles)\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    {
        ifstream f("matrix.txt");
        if (!f.is_open()) {
            std::cerr << "[rank " << rank << "] No se pudo abrir 'matrix.txt'\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // saltar cabecera (ya difundida)
        int dummy;
        f >> dummy >> dummy;

        // saltar todas las filas previas: mi_primera_fila * ncols números
        double tmp;
        const long long to_skip = 1LL * indice_rows[rank] * ncols;
        for (long long i = 0; i < to_skip; ++i) {
            f >> tmp;
            if (!f.good()) {
                std::cerr << "[rank " << rank << "] Error al saltar datos (offset)\n";
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }

        // leer mis elementos: my_nrows * ncols
        for (long long i = 0; i < my_elems; ++i) {
            f >> A_local[i];
            if (!f.good()) {
                std::cerr << "[rank " << rank << "] Error al leer mi bloque local\n";
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        f.close();
    }

    // Número fijo de iteraciones
    const int K = 50;

    std::vector<int> counts_rows(size), displs_rows(size);
    {
        int q   = nrows / size;
        int rem = nrows % size;
        for (int i = 0; i < size; ++i) counts_rows[i] = (i < rem) ? (q + 1) : q;
        displs_rows[0] = 0;
        for (int i = 1; i < size; ++i) displs_rows[i] = displs_rows[i-1] + counts_rows[i-1];
    }

    // Buffers para la iteración
    std::vector<double> b_full(nrows, 0.0);            // b global (reconstruido en cada iter)
    std::vector<double> y_local(my_nrows, 0.0);        // y_local = A_local * b_full

    for (int k = 0; k < K; ++k) {
        // Reunir b0 en b_full: paralelo sobre todos los procesos
        MPI_Allgatherv(
            b0.data(), my_nrows, MPI_DOUBLE,
            b_full.data(), counts_rows.data(), displs_rows.data(), MPI_DOUBLE,
            MPI_COMM_WORLD
        );

        // y_local = A_local * b_full  (matvec distribuida por filas)
        for (int i = 0; i < my_nrows; ++i) {
            double acc = 0.0;
            const long long row_off = 1LL * i * ncols;
            for (int j = 0; j < ncols; ++j) {
                acc += A_local[row_off + j] * b_full[j];
            }
            y_local[i] = acc;
        }

        // Norma ||y|| en paralelo (Allreduce de suma de cuadrados)
        double local_sum = 0.0;
        for (int i = 0; i < my_nrows; ++i) local_sum += y_local[i] * y_local[i];

        double global_sum = 0.0;
        MPI_Allreduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        double norm_y = std::sqrt(global_sum);
        if (norm_y == 0.0) {
            if (rank == 0) std::cerr << "Norma cero en iteración " << k << "\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Normalizar: b_{k+1} (local) = y_local / ||y||
        for (int i = 0; i < my_nrows; ++i) b0[i] = y_local[i] / norm_y;


        // Para el punto 8, calculamos el valor propio máximo estimado con
        // el Cociente de Rayleigh mu = b^T A b = b^T y

        double local_dot = 0.0;
        for (int i = 0; i < my_nrows; ++i) local_dot += b0[i] * y_local[i];
        double mu = 0.0;
        MPI_Allreduce(&local_dot, &mu, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

        if (rank == 0 && (k % 10 == 0 || k == K-1)) {
            std::cout << "[Iter " << k << "] mu ≈ " << mu << "\n";
        }
    }

        MPI_Allgatherv(
        b0.data(), my_nrows, MPI_DOUBLE,
        b_full.data(), counts_rows.data(), displs_rows.data(), MPI_DOUBLE,
        MPI_COMM_WORLD
    );

    // y_local = A_local * b_full
    for (int i = 0; i < my_nrows; ++i) {
        double acc = 0.0;
        const long long row_off = 1LL * i * ncols;
        for (int j = 0; j < ncols; ++j) {
            acc += A_local[row_off + j] * b_full[j];
        }
        y_local[i] = acc;
    }

    // mu = b^T y  (b ya quedó normalizado al final de la última iteración)
    double local_dot = 0.0, mu = 0.0;
    for (int i = 0; i < my_nrows; ++i) local_dot += b0[i] * y_local[i];
    MPI_Allreduce(&local_dot, &mu, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

    // Error con respecto al valor propio máximo exacto (=10)
    if (rank == 0) {
        double err = std::fabs(mu - 10.0);
        std::cout << "\n=== Punto 8 ===\n";
        std::cout << "Valor propio estimado (mu): " << mu << "\n";
        std::cout << "Error |mu - 10|: " << err << "\n";
    }

    delete[] A_local;

    MPI_Finalize();

    return 0;
}