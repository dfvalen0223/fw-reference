# Firmware ESP32-S3 (ESP-IDF)

Controla todos los píxeles de la palabra por una sola salida RMT con DMA,
dibujando los efectos según la **posición física** de cada LED (tomada de
`main/letters_config.h`, que genera `cad/generate.py`).

Verificado: compila sin warnings con **ESP-IDF 5.4.1** y
**espressif/led_strip 3.0.3** para `esp32s3` (binario de ~780 KB, 48 % libre
en la partición de 1.5 MB).

## Compilar y flashear

```bash
cd led-letters/firmware
idf.py set-target esp32s3
idf.py menuconfig          # LED letters → GPIO, límite de corriente, Wi-Fi...
idf.py build flash monitor
```

`led_strip` se descarga solo desde el registro de componentes de Espressif
(`main/idf_component.yml`).

**Siempre regenerar `letters_config.h` con el mismo comando que generó los
STL** (lo hace `generate.py` automáticamente); si no, el mapa de LEDs no
coincide con las letras impresas.

## Configuración (`menuconfig` → *LED letters*)

| Opción | Por defecto | |
|---|---|---|
| Data GPIO | 16 | Salida hacia el level shifter |
| Button GPIO | 0 | Botón BOOT de la placa |
| Current budget (mA) | 4000 | ~80 % de la fuente. El firmware nunca lo supera |
| Default brightness | 128 | 0–255 |
| Frames per second | 50 | |
| Pixel colour order | GRB | Cambiar a RGB si rojo y verde salen invertidos |
| Wi-Fi access point | sí | SSID `LETRAS-LED` |
| Access point password | `cambiar-esta-clave` | **Cambiarla.** Con menos de 8 caracteres la red queda abierta |

## Uso

- **Botón:** pulsación corta = siguiente efecto; larga (0.8 s) = siguiente
  nivel de brillo.
- **Celular:** conectarse a la red `LETRAS-LED` y abrir `http://192.168.4.1`.
  Se elige efecto, color, brillo y velocidad; muestra el consumo estimado.
- La última configuración se guarda en flash y se recupera al encender.

API (la usa la página, sirve para automatizar desde una consola de luces o un
script): `GET /api/state`, `GET /api/set?effect=rainbow&color=ff8800&brightness=120&speed=128`.

## Efectos

| Nombre | |
|---|---|
| `solid` | Toda la palabra de un color |
| `rainbow` | Arcoíris que recorre la palabra de izquierda a derecha según la posición real de cada LED |
| `letter_chase` | Se enciende una letra por vez |
| `wipe` | Barrido de color de izquierda a derecha |
| `breathe` | Respiración (nunca se apaga del todo) |
| `twinkle` | Destellos sobre una base tenue |
| `letters` | Cada letra de un color: verifica el orden de las letras |
| `chain_test` | Un píxel por vez en orden de cadena: verifica el ensamble contra `assembly_*.svg` |

## Estructura

| Archivo | |
|---|---|
| `main/effects.c` | Motor de efectos, C puro (sin ESP-IDF) |
| `main/power.c` | Brillo, gamma 2.2 y limitador de corriente, C puro |
| `main/main.c` | Driver RMT+DMA, botón, bucle de render |
| `main/settings.c` | Estado compartido + persistencia en NVS |
| `main/web.c` | Punto de acceso Wi-Fi + página de control |
| `main/letters_config.h` | **Generado** por `cad/generate.py` |
| `test/` | Tests en host de `effects.c` y `power.c` |

## Tests en host

```bash
make -C led-letters/firmware/test
```

Compila con `-Wall -Wextra -Werror` y AddressSanitizer/UBSan y verifica cada
efecto (incluso con un layout vacío), el limitador de corriente y la gamma.
