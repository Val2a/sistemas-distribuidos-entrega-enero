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

/**
 * Busca una línea (wantedRowIndex) y la guarda en *rowDataBuffer, en
 */
void searchRow(int pid, int prn, int wantedRowIndex, int splitRows, int cols, float *localM, float *restM, float *rowDataBuffer, int *pidBuffer);

/**
 * Distancia euclidea entre dos vectores
 */
float euclideanDistance(float *v1, float *v2, int size);

int main(int argc, char *argv[])
{
    // Variables
    // - Argumentos
    int k;
    char *dataPath;
    int numThreads;
    float sumaMAPE = 0;
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
    int targetRow;
    // --- Datos de la línea objetivo
    float *targetRowData;
    // --- Línea de referencia (la que se usará para buscar vecinos)
    int referenceRow;
    // --- Datos de la línea de referencia
    float *referenceRowData;

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
    FILE *fPred = NULL, *fMape = NULL, *fTiempo = NULL;
    // --- Variable auxiliar
    char fileName[FILENAME_BUFFER];
    // -- Tiempos
    double t_inicio, t_fin;

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
    referenceRowData = (float *)malloc(cols * sizeof(float));
    targetRowData = (float *)malloc(cols * sizeof(float));

    MPI_File_read_at_all(fh, offset, localMatrix, splitRows * cols, MPI_FLOAT, 0);
    printf("[PID: %d] [SPLIT] Offset: %lld, primer float: %0.1f, ultimo float: %0.1f\n", pid, offset, localMatrix[0], localMatrix[splitRows * cols - 1]);

    if (pid == 0)
    {
        // Preparamos los archivos de salida
        fPred = fopen("Predicciones.txt", "w");
        fMape = fopen("MAPE.txt", "w");
        t_inicio = MPI_Wtime();

        if (restRows > 0)
        {
            restMatrix = (float *)malloc(restRows * cols * sizeof(float));
            offset = 8 + (rows - restRows) * cols * sizeof(float);
            MPI_File_read_at(fh, offset, restMatrix, restRows * cols, MPI_FLOAT, 0);
            printf("[PID: %d] [RESTO] Offset: %lld, primer float: %0.1f, ultimo float: %0.1f\n", pid, offset, restMatrix[0], restMatrix[restRows * cols - 1]);
        }
    }

    // Tenemos que realizar las N_PREDICTIONS predicciones.
    // Para ello, debemos recoger la línea actual (referenceRowData)
    // y la línea siguiente (targetRowData, que es la que queremos predecir)
    //
    // Empezamos a contar desde la última, por lo que i sería
    // la línea a predecir e i+1 sería la línea que vamos a usar
    // para encontrar los vecinos y predecir i.
    for (int i = 1; i <= N_PREDICTIONS; i++)
    {
        int pidWithTheRow = -1; // Será sobreescrito
        int referenceRowIndex = rows - i - 1;
        // int targetRowIndex = rows - i;

        searchRow(pid, prn, referenceRowIndex, splitRows, cols, localMatrix, restMatrix,
                  referenceRowData, &pidWithTheRow); // Output

        // Bcast para realizar la distancia euclídea de cada fila local contra la de referencia
        MPI_Bcast(referenceRowData, cols, MPI_FLOAT, pidWithTheRow, MPI_COMM_WORLD);

// Calculamos la distancia de cada fila local con OpenMP
#pragma omp parallel for shared(localCosts, referenceRowData, localMatrix, splitRows, cols, referenceRowIndex, pid) schedule(static)
        for (int i = 0; i < splitRows; i++)
        {
            int globalId = pid * splitRows + i;
            // Solo comparamos con filas pasadas
            if (globalId < referenceRowIndex)
            {
                localCosts[i] = euclideanDistance(referenceRowData, &localMatrix[i * cols], cols);
            }
            else
            {
                localCosts[i] = __FLT_MAX__; // Ignoramos filas futuras
            }
        }

        // Ahora vamos con la recolección de resultados

        float *allCosts = NULL;
        if (pid == 0)
        {
            allCosts = (float *)malloc(rows * sizeof(float));
        }
        // Recogemos en el PID 0 todos los costes calculados
        MPI_Gather(localCosts, splitRows, MPI_FLOAT, allCosts, splitRows, MPI_FLOAT, 0, MPI_COMM_WORLD);

        // Inicializamos el vector de predicciones a 0
        float *prediction = (float *)malloc(cols * sizeof(float));
        for (int j = 0; j < cols; j++)
        {
            prediction[j] = 0.0f;
        }

        // Gestión del resto
        if (pid == 0 && restRows > 0)
        {
            for (int r = 0; r < restRows; r++)
            {
                int globalId = (rows - restRows) + r;
                if (globalId < referenceRowIndex)
                {
                    allCosts[globalId] = euclideanDistance(referenceRowData, &restMatrix[r * cols], cols);
                }
                else
                {
                    allCosts[globalId] = __FLT_MAX__;
                }
            }
        }

        // BÚSQUEDA DE K VECINOS
        for (int n = 0; n < k; n++)
        {
            int bestId = -1;

            // Solo el PID 0 busca quién es el mejor en su array global de costes
            if (pid == 0)
            {
                float min = __FLT_MAX__;
                for (int j = 0; j < rows; j++)
                {
                    if (allCosts[j] < min)
                    {
                        min = allCosts[j];
                        bestId = j;
                    }
                }
                allCosts[bestId] = __FLT_MAX__; // Marcamos para no repetir vecino
            }

            // El PID 0 comunica a todos qué ID de vecino ha elegido
            MPI_Bcast(&bestId, 1, MPI_INT, 0, MPI_COMM_WORLD);

            // Todos preparan el buffer para recibir al sucesor
            float *sucessorData = (float *)malloc(cols * sizeof(float));
            int pidSuccessorOwner;
            int successorId = bestId + 1;

            // Cada proceso busca si tiene el sucesor en su memoria local
            searchRow(pid, prn, successorId, splitRows, cols, localMatrix, restMatrix, sucessorData, &pidSuccessorOwner);

            MPI_Bcast(sucessorData, cols, MPI_FLOAT, pidSuccessorOwner, MPI_COMM_WORLD);

            if (pid == 0)
            {
                for (int j = 0; j < cols; j++)
                {
                    prediction[j] += sucessorData[j];
                }
            }
            free(sucessorData);
        }

        // CÁLCULO DE MAPE Y ERROR
        if (pid == 0)
        {
            for (int j = 0; j < cols; j++)
                prediction[j] /= (float)k;
        }

        // Todos buscan quién tiene la fila REAL (Target) para comparar
        float *realData = (float *)malloc(cols * sizeof(float));
        int tOwner;
        searchRow(pid, prn, rows - i, splitRows, cols, localMatrix, restMatrix, realData, &tOwner);

        // Sincronizamos la fila real para que el PID 0 pueda calcular el MAPE
        MPI_Bcast(realData, cols, MPI_FLOAT, tOwner, MPI_COMM_WORLD);

        if (pid == 0)
        {
            float error = calculateMAPE(prediction, realData, cols);
            if (i % 100 == 0)
            {
                printf("[PID: 0] Predicción [%d] OK. MAPE: %.2f%%\n", i, error); // Mostramos una predicción cada 100 líneas para no sobrecargar la consola.
            }
            sumaMAPE += error;

            // Escribimos las 24 predicciones en una línea
            for (int j = 0; j < cols; j++)
            {
                fprintf(fPred, "%.1f%s", prediction[j], (j == cols - 1) ? "" : ",");
            }
            fprintf(fPred, "\n");

            free(allCosts); // Liberamos el array de costes global en el PID 0
            // Escribimos el MAPE de la prediccion
            fprintf(fMape, "%.4f\n", error);
        }

        // Limpiamos en cada iteración
        free(realData);
        free(prediction);
    }

    if (pid == 0)
    {
        t_fin = MPI_Wtime(); // Fin del cronómetro

        // Generamos Tiempo.txt
        fTiempo = fopen("Tiempo.txt", "w");
        fprintf(fTiempo, "Fichero procesado: %s\n", dataPath);
        fprintf(fTiempo, "Tiempo total     : %.4f segundos\n", t_fin - t_inicio);
        fprintf(fTiempo, "MAPE global      : %.4f%%\n", sumaMAPE / N_PREDICTIONS);
        fprintf(fTiempo, "Configuración    : %d procesos, %d hilos\n", prn, numThreads);

        // Liberamos memoria
        fclose(fPred);
        fclose(fMape);
        fclose(fTiempo);

        printf("\n--------------------------------------------\n");
        printf("FINALIZADO: %s\n", dataPath);
        printf("MAPE PROMEDIO TOTAL: %.4f%%\n", sumaMAPE / N_PREDICTIONS);
        printf("--------------------------------------------\n");
    }

    // Liberamos la memoria asignada
    MPI_File_close(&fh);

    free(localMatrix);
    free(localCosts);
    free(targetRowData);
    free(referenceRowData);
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
    int count = 0;

    // Éste for, paralelizado, empeora los tiempos de ejecución.
    for (int i = 0; i < size; i++)
    {
        // Solo calculamos si el valor real no es cero para evitar inf/nan
        if (fabsf(vReal[i]) > 0.0001f)
        {
            sum += fabsf(vReal[i] - vPredict[i]) / fabsf(vReal[i]);
            count++;
        }
    }

    return (count == 0) ? 0.0f : (sum * 100.0f) / (float)count;
}

void searchRow(int pid, int prn, int wantedRowIndex, int splitRows, int cols, float *localM, float *restM, float *rowDataBuffer, int *pidBuffer)
{
    // Calculamos qué proceso debería tener la fila originalmente
    *pidBuffer = wantedRowIndex / splitRows;
    int rowToSearchIndex;
    float *m;

    // Si el PID calculado es igual o mayor al número de procesos,
    // significa que la fila está en el resto
    if (*pidBuffer >= prn)
    {
        *pidBuffer = 0;
        m = restM;
        // Ajustamos el índice, que sería posición absoluta menos las filas ya repartidas
        rowToSearchIndex = wantedRowIndex - (prn * splitRows);
    }
    else
    {
        m = localM;
        rowToSearchIndex = wantedRowIndex % splitRows;
    }

    // El proceso que tiene los datos los copia al buffer
    if (*pidBuffer == pid)
    {
        // Si paralelizamos el for con openmp, la ejecución resulta ser más lenta, hasta 2 segundos con
        // el fichero datos_10X. No renta.
        for (int j = 0; j < cols; j++)
        {
            rowDataBuffer[j] = m[rowToSearchIndex * cols + j];
        }
    }
}

float euclideanDistance(float *v1, float *v2, int size)
{
    float d = 0.0f;

    // Paralelizar éste for ha resultado en una ejecución casi 10 veces más lenta,
    // por lo que no renta.
    for (size_t i = 0; i < size; i++)
    {
        float diff = v1[i] - v2[i];
        d += diff * diff;
    }

    return sqrtf(d);
}
