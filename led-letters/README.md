# Letras LED modulares para eventos

Generador **automático** de letras corpóreas retroiluminadas (15 cm – 1 m de
alto) impresas en 3D, con píxeles **WS2812B** y un controlador **ESP32-S3**.

Escribes la palabra y la altura; un solo comando produce todo lo necesario
para fabricarla:

```
python3 cad/generate.py --text "HOLA" --letter-height 400 --output-dir build/HOLA
```

| Sale del generador | Para qué |
|---|---|
| `*_carcaza.stl`, `*_diffusor.stl` | Una carcaza opaca + un difusor translúcido por baldosa, listos para el slicer |
| `assembly_<letra>.svg` | Mapa de ensamble: baldosas, orden de los píxeles, extensiones, puntos de amarre |
| `manifest.json`, `bom.csv` | Lista de piezas, píxeles, uniones, consumo máximo, avisos |
| `firmware/main/letters_config.h` | Mapa físico de cada LED para el firmware (se actualiza solo) |

**Cero tornillos, cero pegamento, cero soldaduras dentro de la letra.**

![Letra A de 400 mm: 4 baldosas encastradas](docs/img/A_baldosas.png)

## Cómo funciona

```
 texto + altura ──► generate.py ──► OpenSCAD (2D) ──► análisis en Python:
                                                        · grilla de baldosas que entran en la cama
                                                        · cortes lejos de bordes de la letra
                                                        · posición de cada píxel + orden de la cadena
                                                        · puntos de amarre y nervaduras puente
                        ┌────────────────────────────────┘
                        ▼
               OpenSCAD (3D, en paralelo) ──► STL por baldosa
                        │
                        ├──► manifest.json / bom.csv / assembly_*.svg
                        └──► firmware/main/letters_config.h ──► ESP32-S3 (ESP-IDF)
```

### Decisiones de diseño

- **OpenSCAD headless** (en vez de Blender/FreeCAD): 100 % paramétrico y
  ejecutable por línea de comandos, así el proceso completo no tiene pasos
  manuales. Python hace la parte "inteligente" (dónde cortar, dónde poner cada
  LED) y le pasa coordenadas a OpenSCAD.
- **Letra = grilla de baldosas.** A 60 cm una letra mide ~50–60 cm de ancho,
  así que no alcanza con cortar en franjas: se corta en X e Y para que cada
  baldosa entre en la cama de la impresora (`--bed-x/--bed-y`).
- **Unión a presión continua** (lengüeta + ranura con cordón, como las tapas
  de cajas impresas) que sigue el contorno de la pared de la letra. Funciona
  con cualquier tipografía, se imprime sin soportes y se separa haciendo
  palanca. Detalle en [docs/fabricacion.md](docs/fabricacion.md).
- **Píxeles WS2812B de 12 mm en tira-cable** (no tira adhesiva) encajados en
  zócalos impresos. El generador los distribuye con separación ≈ profundidad
  (difusión pareja, sin puntos calientes) y los trazos angostos reciben una
  línea central. Montar = empujar cada píxel en su zócalo, siguiendo el mapa.
- **Montaje sin tornillos:** precintos (zip-ties) por las ranuras de amarre a
  un truss / bastidor, o cinta Dual-Lock/VHB (el dorso es plano).

## Tamaños de referencia

"HOLA", Liberation Sans Bold, cadena de píxeles con 100 mm de cable entre LEDs.
Cama 220 × 220 mm salvo indicación.

| Altura | Profundidad | Cama | Baldosas | Piezas impresas | Uniones | Píxeles | Paso | Extensiones | Amarres | Consumo máx. (5 V) |
|---|---|---|---|---|---|---|---|---|---|---|
| 150 mm | 40 mm | 220 | 4 | 8 | 0 | 33 | 40 mm | 1 | 0 | 2.0 A |
| 250 mm | 40 mm | 220 | 14 | 28 | 13 | 62 | 40 mm | 2 | 13 | 3.7 A |
| 400 mm | 64 mm | 220 | 15 | 30 | 14 | 83 | 64 mm | 4 | 15 | 5.0 A |
| 600 mm | 96 mm | 220 | 29 | 58 | 30 | 157 | 71 mm | 6 | 29 | 9.4 A |
| 600 mm | 96 mm | 350 | 15 | 30 | 14 | 155 | 71 mm | 6 | 27 | 9.3 A |
| 1000 mm | 150 mm | 220 | 76 | 152 | 105 | 416 | 71 mm | 13 | 68 | 25.0 A |
| 1000 mm | 150 mm | 350 | 29 | 58 | 30 | 407 | 71 mm | 9 | 58 | 24.4 A |

Para letras de 60 cm – 1 m conviene una impresora de cama grande (≥ 350 mm):
**reduce las piezas a menos de la mitad**. Las letras de 150 mm salen de una sola
pieza y se montan con cinta (no tienen espacio para amarres).

## Inicio rápido

Requisitos: `openscad` (2021.01+), Python 3.10+ (sin dependencias externas).

```bash
cd led-letters/cad

# 1. Calibrar la unión una vez por impresora/material (ver docs/fabricacion.md)
openscad -o cupon.stl -D 'part="snap_test_coupon"' -D joint_clearance=0.18 part.scad

# 2. Ver el plan sin generar STL (instantáneo)
python3 generate.py --text "HOLA" --letter-height 600 --bed-x 256 --bed-y 256 \
    --plan-only --output-dir ../build/HOLA

# 3. Generar todo
python3 generate.py --text "HOLA" --letter-height 600 --bed-x 256 --bed-y 256 \
    --joint-clearance 0.18 --jobs 8 --output-dir ../build/HOLA
```

Luego: imprimir → [ensamblar](docs/ensamble.md) → [cablear](docs/electronica.md)
→ [compilar y flashear el firmware](firmware/README.md).

Opciones principales de `generate.py` (`--help` para todas):

| Opción | Por defecto | |
|---|---|---|
| `--letter-height` | — | Altura total de la letra en mm (150–1000) |
| `--depth` | 16 % de la altura, 40–150 mm | Profundidad de la caja |
| `--bed-x/--bed-y/--bed-z` | 220/220/250 | Volumen de impresión |
| `--led-pitch` | ≈ profundidad (máx. cable/1.4) | Separación entre píxeles |
| `--pixel-lead` | 100 | Largo de cable entre píxeles de la tira que compres |
| `--joint-clearance` | 0.18 | Holgura de la unión, del cupón de calibración |
| `--font` | `Liberation Sans:style=Bold` | Cualquier fuente instalada (`fc-list`) |
| `--no-bridge-islands` | — | No unir islas separadas dentro de una baldosa |

## Estructura

```
led-letters/
├── cad/
│   ├── generate.py          # orquestador: plan, píxeles, STL, manifiesto, firmware
│   ├── part.scad            # entrada CLI de OpenSCAD (una baldosa por llamada)
│   ├── channel_letter.scad  # carcaza, difusor, zócalos, amarres, nervaduras
│   └── lib/snap_fit.scad    # uniones a presión + cupón de calibración
├── firmware/                # proyecto ESP-IDF para ESP32-S3
│   ├── main/                # main.c, effects.c, power.c, web.c, settings.c
│   └── test/                # tests en host de efectos y limitador de corriente
└── docs/                    # fabricación, ensamble, electrónica
```

## Validación hecha

- Todas las piezas de "HOLA" a 400 mm (30 STL) y de una "A" de 1 m (40 STL)
  se verificaron automáticamente: **una sola pieza y malla cerrada** (cada
  arista compartida por exactamente 2 caras).
- Sección de la unión verificada (lengüeta/cordón vs. boca/cámara):
  ![Corte de la unión](docs/img/union_corte.png)
- Firmware: compila sin warnings con ESP-IDF 5.4.1 + `led_strip` 3.0.3 para
  ESP32-S3, y tiene tests en host de efectos y limitador de corriente con
  AddressSanitizer/UBSan (`make -C firmware/test`).

## Limitaciones conocidas

- La holgura de la unión depende de la impresora: **imprimir el cupón primero**.
- Las letras con ramas (A, E, H, K…) necesitan alguna extensión de cable en la
  cadena de píxeles; el mapa SVG indica dónde y de qué largo.
- Tipografías muy finas pueden no dejar lugar para los zócalos de 12 mm en
  letras chicas: el generador avisa.
- Las minúsculas con descendentes (g, j, p…) se escalan por su caja completa.
