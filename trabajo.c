#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char *argv[])
{
    int pid, prn;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &prn);

    if (argc < 4)
    {
        if (pid == 0)
        {
            printf("Error: Los argumentos son: <Num. K de vecinos> <Ruta a datos> <Num. procesos> <Num. hilos>\n");
        }
        MPI_Finalize();
        return 0;
    }

    MPI_Finalize();
    return 0;
}