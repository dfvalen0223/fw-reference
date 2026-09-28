# Electrónica y cableado

El manual generado (`manual_ensamble.pdf`) trae los valores concretos de cada
letrero: salida del controlador, fuente y largo de cable de cada letra. Este
documento explica el esquema general.

## Topología

Cada letra es independiente: tiene **su propia fuente de 5 V** (enchufada en
un jack DC en el dorso de la letra) y recibe **un cable de datos** desde el
controlador (conector GX12 en el dorso). El controlador no lleva potencia a
las letras, sólo datos.

```
   Fuente 5 V      Fuente 5 V      Fuente 5 V
       │               │               │
   ┌───┴───┐       ┌───┴───┐       ┌───┴───┐
   │   H   │       │   O   │       │   L   │   ...
   └───┬───┘       └───┬───┘       └───┬───┘
       │ datos+GND     │               │
       └──────────┐    │    ┌──────────┘
             OUT 1│OUT 2  OUT 3
              ┌───┴────┴────┴───┐
              │ ESP32-S3 + 245  │◄── cargador USB-C 5 V
              └─────────────────┘
```

## Controlador

| Componente | Detalle |
|---|---|
| ESP32-S3 DevKitC-1 (N8 / N8R2 / N16R8) | Cualquier ESP32-S3 con 4 MB de flash o más |
| 74AHCT245 (1 hasta 8 letras, 2 hasta 16) | Convierte las salidas de 3.3 V a 5 V. VCC y DIR a 5 V, OE y GND a GND |
| Resistencia 100 Ω por salida | En serie, entre la salida B del 245 y el pin 1 del GX12 |
| GX12 3 pines de panel por salida | Pin 1 DATOS, pin 2 GND, pin 3 libre |
| Cargador USB-C 5 V 1 A | Alimenta sólo el controlador |

Las salidas se generan **en paralelo** con el periférico LCD del ESP32-S3 y
DMA: todas las letras reciben su cuadro al mismo tiempo, sin carga de CPU. Por
eso el bus usa 8 GPIO (hasta 8 letras) o 16 (hasta 16), aunque haya menos
letras: las salidas sin letra quedan en bajo. El periférico también necesita
un pin de reloj (WR) y uno de D/C que **quedan sin conectar**.

Pines por defecto (`idf.py menuconfig` → *LED letters*):

| Salida | OUT 1–8 | OUT 9–16 | WR (sin conectar) | D/C (sin conectar) | Botón |
|---|---|---|---|---|---|
| GPIO | 4, 5, 6, 7, 15, 16, 17, 18 | 8, 9, 10, 11, 12, 13, 14, 21 | 2 | 1 | 0 (BOOT) |

## Dentro de cada letra

| Del puerto | A |
|---|---|
| Jack DC centro (+) | 5 V del píxel 1 (entrada de la tira) |
| Jack DC exterior | GND del píxel 1 |
| GX12 pin 1 | DATOS del píxel 1 |
| GX12 pin 2 | GND |
| Capacitor 1000 µF 10 V | Entre 5 V y GND, en el jack (respetar polaridad) |

Si la letra tiene más de ~100 píxeles, llevar también 5 V y GND desde el jack
al último píxel (el manual lo indica como nota especial).

Letras de trazo muy angosto (letras chicas o tipografías finas) no tienen
lugar para los conectores de panel: el generador deja un agujero de 7 mm para
sacar un cable corto con los conectores en la punta.

## Alimentación

Cada píxel consume hasta **60 mA en blanco total**. El generador elige la
fuente de cada letra (la más chica de 2/3/4/5/6/8/10/15/20 A que cubra ~75 %
del máximo) y el firmware mantiene cada letra **por debajo del 80 %** de su
fuente, atenuando esa letra si hace falta. Por eso cada letra puede usar una
fuente distinta.

| Letra (HOLA 400 mm) | Píxeles | Máx. teórico | Fuente | Límite en firmware |
|---|---|---|---|---|
| H | 27 | 1.6 A | 2 A | 1.6 A |
| O | 23 | 1.4 A | 2 A | 1.6 A |
| L | 14 | 0.8 A | 2 A | 1.6 A |
| A | 19 | 1.1 A | 2 A | 1.6 A |

## Cables de datos

- 2 conductores (datos + GND) de 0,5 mm², o cable de micrófono blindado para
  tramos largos o ambientes con ruido.
- El generador calcula el largo suponiendo el controlador detrás del centro
  de la palabra (+1 m de holgura). Hasta ~5 m funciona sin problemas con el
  74AHCT245 y la resistencia de 100 Ω.
- Todas las masas quedan unidas por el pin 2 de cada cable: no hace falta
  unir las fuentes entre sí.
