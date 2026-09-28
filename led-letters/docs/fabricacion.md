# Fabricación (impresión 3D)

Todo se imprime en FDM: un solo proceso y sin herramientas de corte.

## Materiales

| Pieza | Material | Color | Notas |
|---|---|---|---|
| Carcaza | PETG (recomendado) o PLA | Blanco por dentro = más brillo. Negro/color si la letra se ve de lado | PETG aguanta el calor de un escenario/exterior y flexiona mejor en la unión a presión |
| Difusor | PETG natural/transparente o PLA blanco translúcido | Natural | 1.6 mm de piel; con PETG natural queda difuso sin puntos calientes |
| Base | PETG o PLA | Negro (queda a la vista) | Relleno bajo: es voluminosa pero no lleva esfuerzos grandes |

## Parámetros del slicer

| | Carcaza | Difusor | Base |
|---|---|---|---|
| Orientación | **Apoyada sobre el dorso** (la cara abierta hacia arriba) | **Apoyada sobre la cara** (el reborde hacia arriba) | Apoyada sobre la cara inferior (el STL ya sale así) |
| Soportes | No | No | No |
| Capa | 0.2–0.28 mm | 0.2 mm | 0.28 mm |
| Perímetros | 3 (la pared de 2.4 mm queda maciza) | 2 | 3 |
| Relleno | 15–20 % (sólo afecta el dorso y los collares) | 100 % en la piel (queda sólida por espesor) | 10 % gyroid |
| Boquilla | 0.4 mm (o 0.6 mm para letras ≥ 60 cm, más rápido) | 0.4 mm | 0.6 mm si está disponible |

Ninguna geometría tiene voladizos: la unión, los zócalos de píxel y las
ranuras de amarre se construyen en el plano XY y crecen en Z.

## Calibrar la unión a presión (una vez por impresora + material)

La unión entre piezas es una **lengüeta continua con cordón** que entra en
una **ranura con boca angosta y cámara ancha** en la pieza vecina. El cordón
es más ancho que la boca: al empujar, las paredes flexionan, el cordón pasa y
queda trabado. Para desarmar, se hace palanca con un destornillador plano.

![Corte de la unión](img/union_corte.png)

La holgura (`joint_clearance`) depende de cada impresora. Antes de imprimir
letras completas:

```bash
cd cad
for c in 0.12 0.18 0.24; do
  openscad -o cupon_$c.stl -D 'part="snap_test_coupon"' -D joint_clearance=$c part.scad
done
```

Imprimir los tres cupones (5 minutos cada uno) con el mismo material y
parámetros que las letras, y probar:

| Resultado | Qué hacer |
|---|---|
| No entra o se raja | Subir la holgura |
| Entra sin "click" / se sale solo | Bajar la holgura |
| Entra con presión de la mano, hace click, sale haciendo palanca | ✅ Usar ese valor: `--joint-clearance <valor>` |

El cierre del difusor usa un cordón fijo de 0.2 mm de interferencia; si queda
flojo o duro, ajustar `diffusor_bead` en `cad/part.scad`.

## Tiempos orientativos

Con PETG, boquilla 0.4, capa 0.24 mm: una pieza de carcaza de 200 × 200 ×
64 mm lleva unas 3–4 h y el difusor correspondiente ~1 h. Con varias
impresoras en paralelo, los archivos `*_rXcY_*` son independientes entre sí.

Las letras repetidas (p. ej. las dos "A" de "ALMA") generan un solo juego de
STL: la lista de materiales del manual indica cuántas veces imprimir cada uno
y el filamento aproximado.

## Nomenclatura y códigos

`00_H_r1c0_carcaza.stl` = letra #0 del texto ("H"), pieza **fila 1 (desde
abajo), columna 0 (desde la izquierda)**. `00_H_base_B0a.stl` = segmento de
base de la fila 0 (la de atrás), columna "a" (la de la izquierda).

Cada pieza sale con su código **grabado** (0,6 mm): las carcazas en el dorso
(`H r1c0`), las bases en la cara inferior (`H B0a`). El puerto de conexión
lleva grabado `5V` y `DATOS` junto a cada agujero. No hace falta marcar nada a
mano.
