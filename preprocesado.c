#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_LINE_LENGTH 100000 

// Función auxiliar para generar el nombre del archivo .bin 
void generarNombreSalida(const char *entrada, char *salida);

// Función que se encarga de realizar el preprocesamiento a binario
void procesarArchivo(const char *archivoEntrada);

// MAIN
int main(int argc, char *argv[]) {
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            procesarArchivo(argv[i]);
        }
    }else {
        perror("[ERROR] Inserte el nombre de al menos un archivo.\n");
        return -1;
    }

    return 0;
}

// Desarrollo de las funciones
void generarNombreSalida(const char *entrada, char *salida) {
    strcpy(salida, entrada);
    char *punto = strrchr(salida, '.'); // Busca el último punto del nombre
    if (punto != NULL) {
        *punto = '\0'; 
    }
    strcat(salida, ".bin"); 
}

void procesarArchivo(const char *archivoEntrada) {
    char archivoSalida[1024];
    generarNombreSalida(archivoEntrada, archivoSalida);

    // Abrir el archivo a procesar
    FILE *fEntrada = fopen(archivoEntrada, "r");
    if (!fEntrada) {
        perror("[ERROR] No se pudo abrir el archivo de entrada");
        return;
    }

    // Crear el archivo binario resultante en modo escritura
    FILE *fSalida = fopen(archivoSalida, "wb"); 
    if (!fSalida) {
        perror("[ERROR] No se pudo crear el archivo de salida");
        fclose(fEntrada);
        return;
    }

    int filasTotales = 0;
    int columnas = 0;
    char buffer[MAX_LINE_LENGTH];

    printf("--> Procesando: '%s'\n", archivoEntrada);

    if (fscanf(fEntrada, "%d %d", &filasTotales, &columnas) != 2) {
        fprintf(stderr, "[ERROR] Formato de cabecera incorrecto.\n");
        fclose(fEntrada);
        fclose(fSalida);
        return;
    }

    // Escribir en el archivo binario
    fwrite(&filasTotales, sizeof(int), 1, fSalida);
    fwrite(&columnas, sizeof(int), 1, fSalida);

    fgets(buffer, MAX_LINE_LENGTH, fEntrada);

    // Procesar los datos separados por comas
    uint16_t *filaTemp = (uint16_t*) malloc(columnas * sizeof(uint16_t));
    int contador_filas = 0;

    while (fgets(buffer, MAX_LINE_LENGTH, fEntrada) != NULL) {
        // Tratar las líneas que no estén vacías
        if (strlen(buffer) >= 2){
            char *token = strtok(buffer, ",\n\r"); 
            int col = 0;
            
            while (token != NULL && col < columnas) {
                filaTemp[col] = atof(token); 
                token = strtok(NULL, ",\n\r"); 
                col++;
            }

            // Si la línea tiene datos válidos, se escriben en el archivo binario
            if (col > 0) {
                while (col < columnas) {
                    filaTemp[col] = 0.0;
                    col++;
                }
                fwrite(filaTemp, sizeof(uint16_t), columnas, fSalida);
                contador_filas++;
            }
        }
        
    }

    printf("[OK] Generado: '%s' (%d filas, %d columnas)\n\n", 
           archivoSalida, contador_filas, columnas);

    free(filaTemp);
    fclose(fEntrada);
    fclose(fSalida);
}