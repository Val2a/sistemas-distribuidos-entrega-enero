#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>

/**
 * Comprobamos que los argumentos
 * sean correctos.
 *
 * En caso contrario, finaliza el programa.
 */
int checkArguments(int pid, int argc);

int main(int argc, char *argv[])
{
    // Variables
    // - Argumentos
    int k;
    char *dataPath;
    int numThreads;
    // - MPI
    int pid, prn;
    // - Archivo
    MPI_Offset offset;
    MPI_File fh;
    int rows, cols;
    // - Programa
    int splitRows, restRows;
    // -- Datos del día más actual
    float *currentDayData;
    // -- PID que ha leido el trozo
    //    con el dia actual
    int pidCurrentDayData;
    // -- Matrices de datos
    float *localMatrix;
    float *restMatrix;
    // -- Arrays de costes
    float *costs;
    float *localCosts;
    // -- Arrays de mejores vecinos (index)
    int *bests;
    int *localBests;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &prn);

    // Control de argumentos
    checkArguments(pid, argc);

    k = atoi(argv[1]);
    dataPath = argv[2];
    numThreads = atoi(argv[3]);

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

    splitRows = rows / prn;
    restRows = rows % prn;
    offset = 8 + pid * splitRows * cols * sizeof(float);

    // Realmente, ésto no es una matriz, si no un
    // vector unidimensional. MPI se lleva mejor
    // con éstos que con las matrices, así que los usaremos.
    localMatrix = (float *)malloc(splitRows * cols * sizeof(float));
    localCosts = (float *)malloc(splitRows * sizeof(float));
    currentDayData = (float *)malloc(cols * sizeof(float));

    MPI_File_read_at_all(fh, offset, localMatrix, splitRows * cols, MPI_FLOAT, 0);
    printf("[PID: %d] Offset: %d, primer float: %0.1f, ultimo float: %0.1f\n", pid, offset, localMatrix[0], localMatrix[splitRows * cols - 1]);

    if (pid == 0 && restRows > 0)
    {
        restMatrix = (float *)malloc(restRows * cols * sizeof(float));
        offset = 8 + (rows - restRows) * cols * sizeof(float);
        MPI_File_read_at(fh, offset, restMatrix, restRows * cols, MPI_FLOAT, 0);
        printf("[PID: %d] RESTO: Offset: %d, primer float: %0.1f, ultimo float: %0.1f\n", pid, offset, restMatrix[0], restMatrix[restRows * cols - 1]);
    }

    // Mandamos los datos del día actual (el último)
    // al resto de procesos.
    // La ubicación de éste día puede variar:
    // Si restRows == 0, la tiene el último proceso en localMatrix
    // Si restRows != 0, la tendrá el proceso 0 en restMatrix
    pidCurrentDayData = (restRows > 0) ? 0 : prn - 1;
    if (pid == pidCurrentDayData)
    {
        float *matrix;
        int size;
        if (pid == 0)
        {
            matrix = restMatrix;
            size = restRows;
        }
        else
        {
            matrix = localMatrix;
            size = splitRows;
        }

        for (int i = 0; i < cols; i++)
        {
            currentDayData[i] = matrix[(size - 1) * cols + i];
        }

        printf("[PID: %d] Día actual, linea: %d, primer float: %0.1f, ultimo float: %0.1f\n", pid, rows, currentDayData[0], currentDayData[cols - 1]);
    }

    MPI_Bcast(currentDayData, cols, MPI_FLOAT, pidCurrentDayData, MPI_COMM_WORLD);

        // Liberamos la memoria asignada
    free(localMatrix);
    free(localCosts);
    free(currentDayData);
    if (pid == 0 && restRows > 0)
    {
        free(restMatrix);
    }

    MPI_Finalize();
    return 0;
}

int checkArguments(int pid, int argc)
{
    if (argc < 3)
    {
        if (pid == 0)
        {
            printf("Error: Los argumentos son: -np <Num. procesos> nombrePrograma <Num. K de vecinos> <Ruta a datos> <Num. hilos>\n");
        }
        MPI_Finalize();
        return 0;
    }
}