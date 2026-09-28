# Electrónica y cableado

## Topología

Una sola cadena de datos recorre todas las letras (letra a letra por un
conector JST-SM de 3 pines). La **alimentación no viaja por la cadena**: cada
letra recibe 5 V y GND directamente desde la fuente (inyección por letra).

```
                 ┌──────────── 5 V / GND (cable grueso, fusible por rama) ────────────┐
                 │                         │                          │               │
 ┌──────────┐   ┌┴────────┐   JST-SM      ┌┴────────┐   JST-SM       ┌┴────────┐      │
 │ ESP32-S3 ├──►│ Letra H │──────────────►│ Letra O │───────────────►│ Letra L │─ ... │
 └────┬─────┘   └─────────┘  datos + GND  └─────────┘  datos + GND   └─────────┘      │
      │  GPIO16 ─► 74AHCT125 ─► 330 Ω ─► DATA IN                                    ┌──┴───┐
      └──────────────────────────── GND común ──────────────────────────────────────┤Fuente│
                                                                                    │ 5 V  │
                                                                                    └──────┘
```

## Controlador

| Componente | Detalle |
|---|---|
| ESP32-S3 DevKitC-1 (N8 / N8R2 / N16R8) | Cualquier ESP32-S3 con 4 MB de flash o más |
| 74AHCT125 (o SN74AHCT1G125) | Convierte los datos de 3.3 V a 5 V. Sin esto la primera letra puede parpadear |
| Resistencia 330 Ω | En serie, a la salida del level shifter |
| Capacitor 1000 µF / 10 V | En la entrada de alimentación de **cada letra** (entre 5 V y GND) |
| Pulsador (opcional) | El botón BOOT de la placa (GPIO 0) ya sirve |

Pines por defecto (se cambian en `idf.py menuconfig` → *LED letters*):
datos **GPIO 16**, botón **GPIO 0**. Evitar GPIO 48: en muchas placas S3
maneja el LED RGB integrado.

La placa se puede alimentar desde los mismos 5 V por su pin `5V`. **No
conectar a la vez el USB y los 5 V externos** salvo que la placa tenga diodo
de protección.

## Píxeles

WS2812B de **12 mm tipo "bullet" en tira-cable**, 5 V, IP68, con **100 mm de
cable entre píxeles** (o el largo que indiques con `--pixel-lead`). Vienen con
JST-SM de 3 pines en los extremos. Algunos modelos son orden de color RGB en
vez de GRB: si rojo y verde aparecen cambiados, cambiarlo en `menuconfig`.

La cantidad por letra está en `manifest.json` (`letters[].count`). Comprar un
10 % extra.

## Alimentación

Cada píxel consume hasta **60 mA en blanco total**; el manifiesto da el total
(`amps_at_full_white_5v`). En la práctica los efectos de color usan un 30–50 %
de eso, y el firmware **nunca supera el límite configurado**
(`LETTERS_MAX_CURRENT_MA`): atenúa el cuadro completo si hace falta.

| Instalación | Píxeles | Máx. teórico | Fuente sugerida | `LETTERS_MAX_CURRENT_MA` |
|---|---|---|---|---|
| HOLA 150 mm | 33 | 2 A | 5 V 3 A | 2400 |
| HOLA 400 mm | 83 | 5 A | 5 V 6 A | 4800 |
| HOLA 600 mm | 157 | 9.4 A | 5 V 10 A | 8000 |
| HOLA 1 m | ~410 | 25 A | 5 V 20 A (o 2 × 10 A) | 16000 |

Regla: `LETTERS_MAX_CURRENT_MA` ≈ 80 % de la capacidad de la fuente.

### Cables

| Tramo | Sección |
|---|---|
| Fuente → cada letra (hasta 5 A) | 1 mm² / 18 AWG |
| Tronco de la fuente con varias letras | 2.5 mm² / 14 AWG |
| Datos letra a letra | el del JST-SM (22 AWG), máx. ~3 m entre letras |

- Un **fusible por rama** (5 A para letras de hasta ~80 píxeles).
- Si una letra tiene más de ~120 píxeles (letras de 1 m), inyectar 5 V y GND
  también en el último píxel de esa letra.
- Todas las masas unidas: fuente, ESP32-S3 y cada letra.

## Conector entre letras (JST-SM 3 pines)

| Pin | Señal | Color habitual |
|---|---|---|
| 1 | 5 V | Rojo |
| 2 | Datos | Verde (o blanco) |
| 3 | GND | Blanco (o azul) |

Los colores cambian según el fabricante: **medir con tester antes de conectar**.
La salida de una letra (último píxel) va a la entrada (`DATA IN`) de la
siguiente; en esa unión conectar sólo **datos y GND** si cada letra tiene su
propia inyección de 5 V.

## Lista de materiales electrónica (instalación "HOLA" 400 mm)

| Cant. | Ítem |
|---|---|
| 1 | ESP32-S3 DevKitC-1 |
| 1 | 74AHCT125 + resistencia 330 Ω |
| 4 | Capacitor 1000 µF 10 V (uno por letra) |
| 1 | Fuente 5 V 6 A |
| 90 | Píxeles WS2812B 12 mm, tira con 100 mm entre píxeles (83 + 10 %) |
| 4 | Pares JST-SM 3 pines (entrada de cada letra) |
| 4 | Fusibles 5 A con porta-fusible |
| — | Cable 1 mm² rojo/negro, extensiones de 3 hilos según el mapa de ensamble |
| 15 | Precintos ≤ 4.8 mm (según `zip_tie_points`) |
