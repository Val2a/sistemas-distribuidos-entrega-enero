# Trabajo

## 0.- Preprocesado

El primer problema con el que nos encontramos es que el archivo de datos viene en texto plano.

El texto plano es un formato muy ineficiente a la hora de ser leído computacionalmente, ya que el tamaño de cada línea es variable, por lo que no podemos empezar a leer por una "línea" concreta (habría que contar previamente todos los `\n`).

Por tanto, vemos importante aplicar un preprocesado al archivo, de forma que pasemos todos los números de formato texto a formato binario.

Ésto hace que la primera ejecución sea más costosa, ya que habría que leer el archivo en texto, pasarlo a binario, y pasarlo por el algoritmo. Sin embargo, en las siguientes ejecuciones ya podríamos hacer uso del archivo binario, lo cuál haría que el algoritmo fuese mucho más rápido.

También cabe destacar que, pese a que el archivo está en formato texto, todas las líneas excepto la primera miden lo mismo. Seguramente el profesor haya tenido todo lo expuesto en cuenta, y no nos haya querido sobrecargar de trabajo.

De todas formas, en el mundo real siempre se hace preprocesado y se trata con valores binarios, por lo que no tendría sentido que no lo hiciésemos.

## 1.- Calcular los costes respecto al día actual

Se debe de hacer un cálculo de los costes de todas las filas respecto al día actual (podemos contar como que es el último registrado o que podamos seleccionar un día pasado).

Para ello, lo suyo sería dividir el documento en "chunks" usando MPI y que cada proceso, usando OpenMP, calcule el coste de cada fila.

También habría que guardar en un archivo MAPE.txt el error cometido por cada fila.

## 2.- Elegir a los vecinos

Para elegir a los vecinos, se deben de buscar aquellos K vecinos con menor error.

Tenemos que decidir cómo hacer la búsqueda:

### 2.1.- Dividir en K bloques y coger el mejor

Podríamos dividir por segunda vez la tarea en bloques, y hacer la búsqueda de forma paralela.

Ventajas:

- Se paraleliza fácil
- En general es sencillo de implementar
- Mezclaríamos valores de días dispares, potencialmente dando una predicción menos "overfitted".

Desventajas:

- En el caso de que los mejores vecinos estén en el mismo bloque, sólo seleccionaríamos a uno, potencialmente dejando a mejores candidatos atrás.

### 2.2.- Usar un algoritmo de ordenación

Podríamos usar un algoritmo de ordenación que esté optimizado para entornos multihilo. De ésta forma, la ordenación sería distribuida y la búsqueda podría hacerse de forma secuencial seleccionando a los K mejores.

Ventajas:

- La búsqueda se haría de forma secuencial, no paralelizada, lo cual es muy sencillo. Cogeríamos los K mejores de un set ordenado, lo cuál también es muy eficiente.

Desventajas:

- Implementar el algoritmo de ordenación puede ser bastante difícil, ya que aunque ordenemos por bloques, habría que ordenar otra vez de forma global.
- Si los días son muy cercanos entre sí, nuestro algoritmo estaría un poco "overfitted": sólo cogeriamos los mejores valores de un bloque.

### 2.3- Dividir en K bloques y seleccionar los K mejores del bloque

Podríamos hacer lo mismo que en el apartado 2.1 pero, en vez de dividir en K bloques y coger el mejor de cada uno, cogeríamos los K mejores.

Más tarde recogeríamos esos K mejores de cada bloque en uno de los procesos y seleccionaríamos los K mejores de entre todos.

Ventajas:

- Sólo es ligeramente más difícil de implementar que el 2.1, y nos permite conseguir los mejores de todo el archivo.
- No tenemos que meternos en faena con algoritmos de ordenación.

Desventajas:

- Lo mismo que la ventaja: un poco más complicado que el 2.1.
- El 2.1 tenía la ventaja de comparar entre días más dispares a lo largo del tiempo, éste algoritmo no. Aunque eso tampoco tiene por qué ser una característica deseada.

## 3.- Hacer la media del día siguiente a los mejores

Una vez tengamos a los vecinos seleccionados, tendríamos que hacer la media de los días posteriores a los vecinos, y usar el resultado como predicción.

## 4.- Medir el tiempo de ejecución

Se debe de generar un fichero de salida llamado _Tiempo.txt_, que deberá contener el tiempo de ejecución, así como otro archivo con las predicciones.
