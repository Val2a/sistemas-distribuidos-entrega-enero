#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char *argv[])
{
    int pid, prn;
    MPI_Offset offset;
    MPI_File fh;

    // Variables para guardar argumentos
    int k;
    char *dataPath;
    int numThreads;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &prn);

    if (argc < 3)
    {
        if (pid == 0)
        {
            printf("Error: Los argumentos son: -np <Num. procesos> <Num. K de vecinos> <Ruta a datos> <Num. hilos>\n");
        }
        MPI_Finalize();
        return 0;
    }

    if (pid == 0)
    {
        // Ponemos los argumentos en su sitio
        k = atoi(argv[1]);
        dataPath = argv[2];
        numThreads = atoi(argv[3]);
    }

    // Mandamos los argumentos a todos
    // podríamos calcularlos por separado ya que no es muy costoso,
    // pero creo que queda más elegante así.
    MPI_Bcast(k, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(dataPath, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(numThreads, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Abrimos el archivo (todos lo hacen)
    MPI_File_open(MPI_COMM_WORLD, dataPath, MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);

    offset = pid * sizeof(double) * 24;

    MPI_File_read_at_all(fh, offset, lineBuffer, columns, MPI_DOUBLE, 0);
    MPI_Finalize();
    return 0;
}