# Ensamble

Por letra: encastrar baldosas → colocar píxeles → cerrar difusores → colgar.
Sin herramientas salvo una pinza de corte para los precintos.

![Mapa de ensamble de la "A"](img/mapa_ensamble_A.png)

El generador produce un `assembly_<letra>.svg` por letra (vista frontal) con:

- **Rojo punteado:** baldosas y su código `rXcY`.
- **Círculos numerados:** zócalos de píxel, en el orden en que los recorre la
  tira de píxeles. `DATA IN` marca el primero.
- **Naranja punteado `+NNN mm`:** tramos donde el cable entre dos píxeles
  consecutivos es más largo que el de la tira: agregar una extensión de ese
  largo (3 hilos) siguiendo el recorrido dibujado.
- **Rectángulos negros:** puntos de amarre para precintos.

## 1. Encastrar las baldosas

1. Apoyar las baldosas boca abajo (dorso hacia arriba) sobre una mesa, en la
   posición del mapa.
2. Unir **primero cada fila** de izquierda a derecha: la baldosa de la
   izquierda tiene la lengüeta en su borde derecho, la de la derecha la ranura.
   Alinear y presionar a lo largo de la costura hasta sentir el click.
3. Unir **las filas entre sí** de abajo hacia arriba (lengüeta arriba, ranura
   abajo).

Las esquinas donde se cruzan dos cortes no llevan unión (dejan paso a las
uniones perpendiculares): es normal ver ahí una pequeña separación.

## 2. Colocar los píxeles

1. Dar vuelta la letra (cara abierta hacia arriba).
2. Empezando por el zócalo `1` (`DATA IN`), empujar cada píxel en su zócalo
   siguiendo la numeración. Los cables salen por cualquiera de las 4 ranuras
   del zócalo; dejarlos apoyados sobre el fondo.
3. Donde el mapa marca una extensión, intercalar el cable del largo indicado.
4. El extremo `DATA IN` de la tira queda con su conector JST-SM de entrada; el
   último píxel con el de salida (va a la letra siguiente).

Para sacar un píxel (mantenimiento): empujarlo desde atrás con un lápiz por
el agujero de servicio del centro del zócalo.

## 3. Probar antes de cerrar

Conectar el controlador y elegir el efecto **`chain_test`** en la página web
(o con el botón): enciende un píxel por vez, en orden de la cadena, 4 por
segundo. Debe recorrer los números del mapa en orden; si salta, hay un píxel
en el zócalo equivocado. El efecto **`letters`** pinta cada letra de un color
para verificar que el orden de las letras es el correcto.

## 4. Cerrar con los difusores

Cada difusor lleva el mismo código `rXcY` que su carcaza. Apoyarlo sobre la
boca y presionar el borde hasta que el cordón haga click en la ranura interior
de la pared. Se retira con la uña o una espátula plástica.

## 5. Colgar / fijar

Sin tornillos. Dos opciones, según el lugar:

- **Truss o bastidor:** pasar un precinto por las dos ranuras de cada punto de
  amarre (sale por una, rodea el caño, entra por la otra). Usar precintos de
  4.8 mm de ancho o menos.
- **Pared, panel o escenografía:** el dorso es plano; usar Dual-Lock 3M
  (reutilizable) o cinta VHB. Las letras de 150 mm no tienen amarres: usar
  esta opción.

## Transporte

Las letras se transportan **armadas** (la tira de píxeles atraviesa las
costuras). Las baldosas sólo se separan para reparar: soltar primero el
difusor, sacar los píxeles de esa baldosa y luego hacer palanca en la costura.
