#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 65536 

// Función auxiliar para generar el nombre del archivo .bin 
void generar_nombre_salida(const char *entrada, char *salida);

// Función que se encarga de realizar el preprocesamiento a binario
void procesar_archivo(const char *archivo_entrada);

int main(int argc, char *argv[]) {
    // Procesamos todos los archivos pasado por línea de comandos
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            procesar_archivo(argv[i]);
        }
    }else {
        perror("[ERROR] Inserte el nombre de al menos un archivo.\n");
        return -1;
    }

    return 0;
}

// -------- DESARROLLO DE LAS FUNCIONES --------

void generar_nombre_salida(const char *entrada, char *salida) {
    strcpy(salida, entrada);
    char *punto = strrchr(salida, '.'); // Busca el último punto del nombre
    if (punto != NULL) {
        *punto = '\0'; // Cortamos en el punto (ejemplo: datos.txt --> datos )
    }
    strcat(salida, ".bin"); // Añadimos la nueva extensión (ejemplo: datos --> datos.bin)
}

void procesar_archivo(const char *archivo_entrada) {
    char archivo_salida[1024];
    generar_nombre_salida(archivo_entrada, archivo_salida);

    // Abrimos el archivo a procesar
    FILE *f_in = fopen(archivo_entrada, "r");
    if (!f_in) {
        perror("[ERROR] No se pudo abrir el archivo de entrada");
        return;
    }

    // Creamos el archivo binario de salida resultante en modo escritura
    FILE *f_out = fopen(archivo_salida, "wb"); 
    if (!f_out) {
        perror("[ERROR] No se pudo crear el archivo de salida");
        fclose(f_in);
        return;
    }

    int filas_totales = 0;
    int columnas = 0;
    char buffer[MAX_LINE_LENGTH];

    printf("--> Procesando: '%s'\n", archivo_entrada);

    // 1) TRATAMIENTO DE LA CABECERA (Formato: "FILAS COLUMNAS" con espacio)
    if (fscanf(f_in, "%d %d", &filas_totales, &columnas) != 2) {
        fprintf(stderr, "[ERROR] Formato de cabecera incorrecto.\n");
        fclose(f_in);
        fclose(f_out);
        return;
    }

    // 2) ESCRIBIMOS EN BINARIO
    // Escribimos N y K al principio para que el programa principal sepa cuánto leer
    fwrite(&filas_totales, sizeof(int), 1, f_out);
    fwrite(&columnas, sizeof(int), 1, f_out);

    // Consumimos el resto de la primera línea (salto de línea)
    fgets(buffer, MAX_LINE_LENGTH, f_in);

    // 3. PROCESAMIENTO DE LOS DATOS (Formato: con comas)
    double *fila_temp = (double*) malloc(columnas * sizeof(double));
    int contador_filas = 0;

    while (fgets(buffer, MAX_LINE_LENGTH, f_in) != NULL) {
        // Tratamos las líneas que no estén vacías
        if (strlen(buffer) >= 2){
            char *token = strtok(buffer, ",\n\r"); // Separar por coma o salto de línea
            int col = 0;
            
            while (token != NULL && col < columnas) {
                fila_temp[col] = atof(token); // Pasamos de ASCII a float
                token = strtok(NULL, ",\n\r"); 
                col++;
            }

            // Si la línea tiene datos válidos, la escribimos en el binario
            if (col > 0) {
                // Si faltan columnas, rellenamos con 0 por seguridad
                while (col < columnas) {
                    fila_temp[col] = 0.0;
                    col++;
                }
                fwrite(fila_temp, sizeof(double), columnas, f_out);
                contador_filas++;
            }
        }
        
    }

    printf("[OK] Generado: '%s' (%d filas, %d columnas)\n\n", 
           archivo_salida, contador_filas, columnas);

    free(fila_temp);
    fclose(f_in);
    fclose(f_out);
}