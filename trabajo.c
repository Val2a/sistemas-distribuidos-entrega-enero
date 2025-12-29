#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>
#include <math.h>

#define FILENAME_BUFFER 50
#define N_PREDICTIONS 1000

/**
 * Comprobamos que los argumentos
 * sean correctos.
 *
 * En caso contrario, finaliza el programa.
 */
int checkArguments(int pid, int argc);

/**
 * Calcula el mape de un vector predicho respecto
 * a otro vector "real".
 */
float calculateMAPE(float *vPredict, float *vReal, int size);

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
    // --- Línea objetivo (la que se pretende predecir)
    float *targetRow;
    // --- PID que contiene la targetRow
    int targetRowPID;
    // --- Línea de referencia (la que se usará para buscar vecinos)
    float *referenceRow;
    // --- PID que contiene la referenceRow
    int referenceRowPID;
    // -- Matrices de datos
    float *localMatrix;
    float *restMatrix;
    // -- Arrays de costes
    float *costs;
    float *localCosts;
    // -- Arrays de mejores vecinos (index)
    int *bests;
    int *localBests;
    // -- Archivos de salida
    char outputFolder[] = "output/";
    char prediccionesFileName[] = "Predicciones.txt";
    char mapeFileName[] = "MAPE.txt";
    char tiempoFileName[] = "Tiempo.txt";
    // --- Variable auxiliar
    char fileName[FILENAME_BUFFER];

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

    if (N_PREDICTIONS + 1 > rows)
    {
        if (pid == 0)
        {
            printf("Error: El número de predicciones debe de ser menor que el número de filas del archivo.\n");
            MPI_File_close(&fh);
            MPI_Finalize();
            return 0;
        }
    }

    splitRows = rows / prn;
    restRows = rows % prn;
    offset = 8 + pid * splitRows * cols * sizeof(float);

    // Realmente, ésto no es una matriz, si no un
    // vector unidimensional. MPI se lleva mejor
    // con éstos que con las matrices, así que los usaremos.
    localMatrix = (float *)malloc(splitRows * cols * sizeof(float));
    localCosts = (float *)malloc(splitRows * sizeof(float));
    targetRow = (float *)malloc(cols * sizeof(float));

    MPI_File_read_at_all(fh, offset, localMatrix, splitRows * cols, MPI_FLOAT, 0);
    printf("[PID: %d] Offset: %d, primer float: %0.1f, ultimo float: %0.1f\n", pid, offset, localMatrix[0], localMatrix[splitRows * cols - 1]);

    if (pid == 0 && restRows > 0)
    {
        restMatrix = (float *)malloc(restRows * cols * sizeof(float));
        offset = 8 + (rows - restRows) * cols * sizeof(float);
        MPI_File_read_at(fh, offset, restMatrix, restRows * cols, MPI_FLOAT, 0);
        printf("[PID: %d] RESTO: Offset: %d, primer float: %0.1f, ultimo float: %0.1f\n", pid, offset, restMatrix[0], restMatrix[restRows * cols - 1]);
    }

    // Tenemos que realizar las N_PREDICTIONS predicciones.
    // Para ello, debemos recoger la línea actual (referenceRow)
    // y la línea siguiente (targetRow, que es la que queremos predecir)
    //
    // Empezamos a contar desde la última, por lo que i sería
    // la línea a predecir e i+1 sería la línea que vamos a usar
    // para encontrar los vecinos y predecir i.
    for (int i = 0; i < N_PREDICTIONS; i++)
    {
        int rowToSearch = rows - i;
        // Comprobamos si la targetRow está en
        // las líneas de split o resto
        if (restRows == 0 || rowToSearch < (rows - restRows))
        {
            // Si está en split, calculamos que proceso tiene la línea.
            int pidWithTheRow = -1;
            for (int j = prn - 1; j <= 0; j--)
            {
                int firstRowInPid = j * splitRows + 1;  // El PID 0 tendrá como primera la row 1: 0 * splitRows + 1;
                int lastRowInPid = (j + 1) * splitRows; // Si splitRows = 300, el PID 0 tendrá como última la 300 = (0+1) * 300

                if (rowToSearch >= firstRowInPid && rowToSearch <= lastRowInPid)
                {
                    pidWithTheRow = j;
                }
            }
        }
        else
        {
            // En cuyo caso habrá que consultar al proceso maestro
            if (pid == 0)
            {
                for (int j = 0; j < cols; j++)
                {
                    targetRow[j] = restMatrix[(restRows - i) * cols + j];
                }
            }

            MPI_Bcast(targetRow, cols, MPI_FLOAT, 0, MPI_COMM_WORLD);
        }
    }

    // Liberamos la memoria asignada
    MPI_File_close(&fh);

    free(localMatrix);
    free(localCosts);
    free(targetRow);
    free(referenceRow);
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

float calculateMAPE(float *vPredict, float *vReal, int size)
{
    float sum = 0.0;
    for (int i = 0; i < size; i++)
    {
        sum += fabsf(vReal[i] - vPredict[i]) / fabsf(vReal[i]);
    }

    return (sum * 100.0f) / (float)size;
}