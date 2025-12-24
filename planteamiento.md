# Trabajo

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

### 2.2.-Usar un algoritmo de ordenación

Podríamos usar un algoritmo de ordenación que esté optimizado para entornos multihilo. De ésta forma, la ordenación sería distribuida y la búsqueda podría hacerse de forma secuancial seleccionando a los K mejores.

Ventajas:

- La búsqueda se haría de forma secuencial, no paralelizada, lo cual es muy sencillo. Cogeríamos los K mejores de un set ordenado, lo cuál también es muy eficiente.

Desventajas:

- Implementar el algoritmo de ordenación puede ser bastante difícil, ya que aunque ordenemos por bloques, habría que ordenar otra vez de forma global.
- Si los días son muy cercanos entre sí, nuestro algoritmo estaría un poco "overfitted": sólo cogeriamos los mejores valores de un bloque.

## 3.- Hacer la media del día siguiente a los mejores

Una vez tengamos a los vecinos seleccionados, tendríamos que hacer la media de los días posteriores a los vecinos, y usar el resultado como predicción.
