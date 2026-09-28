# Firmware ESP32-S3 (ESP-IDF)

Un solo ESP32-S3 controla todas las letras, **una salida por letra** (OUT 1,
OUT 2…), enviadas en paralelo por el periférico LCD del chip con DMA: hasta 8
letras con un bus de 8 líneas y hasta 16 con uno de 16. Los efectos se dibujan
según la **posición física** de cada LED y cada letra se mantiene dentro del
límite de corriente de su propia fuente. Todo sale de `main/letters_config.h`,
que genera `cad/generate.py`.

Verificado: compila sin warnings con **ESP-IDF 5.4.1** para `esp32s3`
(binario de ~770 KB, 49 % libre en la partición de 1.5 MB), sin componentes
externos. **No probado todavía con hardware**: antes del primer evento,
verificar una salida con un analizador lógico o una letra chica.

## Compilar y flashear

```bash
cd firmware
idf.py set-target esp32s3
idf.py menuconfig          # LED letters → pines de salida, Wi-Fi...
idf.py build flash monitor
```

**Siempre regenerar `letters_config.h` con el mismo comando que generó los
STL** (lo hace `generate.py` automáticamente); si no, el mapa de LEDs no
coincide con las letras impresas.

## Configuración (`menuconfig` → *LED letters*)

| Opción | Por defecto | |
|---|---|---|
| Output GPIOs | `4,5,6,7,15,16,17,18,8,9,10,11,12,13,14,21` | OUT 1 primero. Se usan los primeros 8 (hasta 8 letras) o los 16 |
| LCD bus WR / D/C GPIO | 2 / 1 | Los necesita el periférico; **quedan sin conectar** |
| Button GPIO | 0 | Botón BOOT de la placa |
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
| `main/main.c` | Bucle de render, límite de corriente por letra, botón |
| `main/lanes.c` | Codificador de las salidas paralelas, C puro |
| `main/output.c` | Envío por el bus LCD i80 con DMA |
| `main/settings.c` | Estado compartido + persistencia en NVS |
| `main/web.c` | Punto de acceso Wi-Fi + página de control |
| `main/letters_config.h` | **Generado** por `cad/generate.py` |
| `test/` | Tests en host de `effects.c`, `power.c` y `lanes.c` |

## Tests en host

```bash
make -C firmware/test
```

Compila con `-Wall -Wextra -Werror` y AddressSanitizer/UBSan y verifica cada
efecto (incluso con un layout vacío), el limitador de corriente, la gamma y
el codificador paralelo (decodifica cada salida y comprueba sus bytes y la
forma de cada bit, en buses de 8 y 16 líneas con letras de distinto largo).

## Temporización de la salida

Cada bit WS2812 se envía en 4 ranuras de 312,5 ns (bus a 3,2 MHz): un `0` es
312 ns en alto y un `1` 625 ns en alto, 1,25 µs por bit. Cumple con los
WS2812B-V5 actuales. Un cuadro de 110 LEDs por letra tarda 3,3 ms en salir.
