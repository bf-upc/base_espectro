# Launcher — Consola ESP32-S3

Firmware base de la consola. Siempre arranca primero y decide
qué hacer según el estado del botón B al encender.

## Flujo de arranque

```
Encender consola
      │
      ├─ Botón B pulsado ──► GAME LOADER
      │                          │
      │                     Crea red WiFi "Consola-ESP32"
      │                     Abre http://192.168.4.1
      │                     Sube el .bin desde el navegador
      │                     Reinicia con el juego nuevo
      │
      └─ Botón B libre ───► Arrancar juego instalado
                                 │
                            Sin juego ──► Pantalla de aviso
```

## Cómo instalar el launcher

1. Abre este proyecto en PlatformIO
2. Flashea con `pio run --target upload`
3. La primera vez la consola mostrará "Sin juego instalado"

## Cómo subir un juego

1. Enciende la consola **manteniendo el botón B pulsado**
2. La pantalla muestra la red WiFi y la IP
3. Conéctate a la red **Consola-ESP32** (contraseña: **gameloader**)
4. Abre el navegador en **http://192.168.4.1**
5. Selecciona el `.bin` de tu juego y pulsa "Subir juego"
6. La consola reinicia y arranca el juego

## Dónde encontrar el .bin de tu juego

Después de compilar con PlatformIO, el binario está en:
```
.pio/build/<nombre_entorno>/firmware.bin
```

## Cómo desarrollar un juego compatible

El juego debe compilarse con la **misma tabla de particiones**
(`partitions_ota.csv`) para que el tamaño de los slots coincida.

Copia el archivo `partitions_ota.csv` a tu proyecto de juego
y añade en su `platformio.ini`:
```ini
board_build.partitions = partitions_ota.csv
```

## Pines utilizados por el launcher

| GPIO | Función        |
|------|----------------|
| 41   | Botón B        |
| 10   | TFT CS         |
| 7    | TFT DC         |
| 6    | TFT RST        |
| 15   | TFT BL (PWM)   |
| 11   | SPI MOSI       |
| 47   | SPI MISO       |
| 12   | SPI SCLK       |

## Volver al launcher desde un juego

El juego puede llamar a `esp_ota_set_boot_partition(ota_0)`
y `esp_restart()` para volver al launcher. O simplemente
mantener el botón B al reiniciar.
