#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>

int main(int argc, char *argv[])
{
    // Variables
    // 1. Argumentos
    int k;
    char *dataPath;
    int numThreads;
    // 2. MPI
    int pid, prn;
    // 3. Archivo
    MPI_Offset offset;
    MPI_File fh;
    int rows, cols;
    // 4. Programa
    int splitSize, restSize;
    double *matrix;
    double *buffer;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &prn);

    if (argc < 3)
    {
        if (pid == 0)
        {
            printf("Error: Los argumentos son: -np <Num. procesos> nombrePrograma <Num. K de vecinos> <Ruta a datos> <Num. hilos>\n");
        }
        MPI_Finalize();
        return 0;
    }

    k = atoi(argv[1]);
    dataPath = argv[2];
    numThreads = atoi(argv[3]);
    printf("Datapath: %s\n", dataPath);

    printf("[PID: %d]: k:%d\n", pid, k);

    omp_set_num_threads(numThreads);

    // Abrimos el archivo (todos lo hacen)
    MPI_File_open(MPI_COMM_WORLD, dataPath, MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);

    if (pid == 0)
    {
        // Obtenemos el número de filas y columnas
        // Cada uno de esos dos números ocupa exactamente
        // 32 bits en memoria, por eso usamos éste tipo
        // en específico.
        MPI_File_read(fh, &rows, 1, MPI_INT32_T, 0);
        MPI_File_read(fh, &cols, 1, MPI_INT32_T, 0);
    }

    MPI_Bcast(&rows, 1, MPI_INT32_T, 0, MPI_COMM_WORLD);
    MPI_Bcast(&cols, 1, MPI_INT32_T, 0, MPI_COMM_WORLD);

    // Numero de lineas para cada proceso
    splitSize = rows / prn;
    // Lineas restantes
    restSize = rows % prn;

    offset = 8 + pid * splitSize * cols * sizeof(double);

    buffer = (double *)malloc(splitSize * cols * sizeof(double));

    MPI_File_read_at_all(fh, offset, buffer, splitSize * cols, MPI_DOUBLE, 0);

    printf("[PID: %d] Offset: %d, primer double: %0.2f\n", pid, offset, buffer[0]);
    free(buffer);
    MPI_Finalize();
    return 0;
}