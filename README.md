# ESPectro — Template de Joc

Template per desenvolupar jocs per a la **consola portàtil ESPectro** basada en ESP32-S3.

---

## Índex

1. [Arquitectura del sistema](#arquitectura-del-sistema)
2. [Requisits](#requisits)
3. [Instruccions per a humans](#instruccions-per-a-humans)
4. [Instruccions per a LLMs](#instruccions-per-a-llms)
5. [API de hardware](#api-de-hardware)
6. [Dashboard i MCP](#dashboard-i-mcp)
7. [Exemple mínim](#exemple-mínim)
8. [Pujar el joc a la consola](#pujar-el-joc-a-la-consola)

---

## Arquitectura del sistema

```
firmware.bin = launcher + joc (tot en un sol fitxer)

Arrenca
  → Splash screen ESPectro (logo + jingle)
  → Menú principal
        │
        ├─ Botó A → runGame()  ← AQUÍ VA EL TEU JOC
        │              │
        │              └─ return → torna al menú
        │
        └─ Botó B → Game Loader (WiFi AP)
                        │
                        ├─ Dashboard amb rècords de tots els jocs
                        ├─ Puja nou .bin via navegador
                        └─ Botó A → torna al menú
```

**FreeRTOS — 3 tasques concurrents:**

| Tasca | Core | Prioritat | Funció |
|-------|------|-----------|--------|
| `musicTask` | 0 | 2 | Àudio I2S continu |
| `wifiTask` | 0 | 1 | Servidor web en segon pla |
| `loop()` | 1 | — | Joc, menú, lògica |

**Sincronització:**
- `audioQueue` (Queue) — efectes de so des del joc a `musicTask`
- `recordMutex` (Mutex) — protegeix NVS entre `wifiTask` i `runGame()`

**WiFi sempre actiu** des de l'arrencada — el dashboard és accessible a `http://192.168.4.1` en tot moment, fins i tot mentre jugues.

---

## Requisits

**Hardware:**
- ESP32-S3-DevKitC-1 N16R8
- Pantalla TFT ILI9488 4" (SPI)
- Joystick analògic HW-504
- 2 botons tactils 6×6mm
- Amplificador I2S MAX98357A + altavoz 8Ω

**Software:**
- PlatformIO
- Llibreria LovyanGFX `^1.1.12`

**`platformio.ini` mínim:**
```ini
[env:consola]
platform    = espressif32
board       = rymcu-esp32-s3-devkitc-1
framework   = arduino
monitor_speed = 115200
lib_deps =
    lovyan03/LovyanGFX @ ^1.1.12
```

---

## Instruccions per a humans

### Pas 1 — Copia la template
Copia `template_joc.cpp` al teu projecte PlatformIO com a `src/main.cpp`.

### Pas 2 — Omple els TODO

Cerca les línies marcades amb `// TODO:` al fitxer. Són exactament 4:

#### 2.1 Clau del rècord
```cpp
#define RECORD_KEY "nom_joc"  // TODO: canvia això!
```
Posa el nom del teu joc en minúscules i sense espais. Exemple:
```cpp
#define RECORD_KEY "snake"
```
Aquesta clau s'usa per guardar el rècord a la NVS i apareix automàticament al dashboard.

#### 2.2 Títol del menú
```cpp
const char* linia1 = "NOM";   // TODO: primera línia del títol
const char* linia2 = "JOC";   // TODO: segona línia del títol
```

#### 2.3 Variables globals del joc
```cpp
// TODO: VARIABLES GLOBALS DEL JOC
```

#### 2.4 Lògica del joc dins de `runGame()`
```cpp
void runGame() {
    // TODO: implementa el teu joc aquí
}
```

### Pas 3 — Guarda el rècord correctament
Usa el mutex per protegir l'accés a la NVS:
```cpp
if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    saveRecord(score);  // guarda SEMPRE, no només si és rècord
    xSemaphoreGive(recordMutex);
}
```
`saveRecord()` ja gestiona internament si ha de actualitzar el màxim o no.

### Pas 4 — Compila i puja
```
pio run --target upload
```
O via Game Loader (veure [Pujar el joc a la consola](#pujar-el-joc-a-la-consola)).

---

## Instruccions per a LLMs

> Aquesta secció explica com generar un joc complet per a la consola ESPectro a partir de la template.

### Regles obligatòries

1. **NO modificar** cap secció marcada com `// NO MODIFICAR`. Inclou: configuració de pantalla, pins, WiFi, FreeRTOS, Game Loader, menú principal, `setup()`, `loop()`.

2. **Sempre fer `return`** al final de `runGame()` per tornar al menú. Mai usar `while(true)` sense condició de sortida.

3. **`RECORD_KEY`** ha de ser una string única per joc, en minúscules, sense espais ni caràcters especials. Exemples vàlids: `"snake"`, `"pong"`, `"tetris_v2"`.

4. **No usar `delay()` llargs** dins del bucle de joc. Usar `millis()` per controlar el timing.

5. **La pantalla és de 320×480 píxels**, orientació vertical (portrait), rotació 2. L'origen (0,0) és a la cantonada superior esquerra.

6. **L'àudio és síncron** — `playTone()` bloqueja l'execució. Usar-lo amb moderació o implementar una `musicTask` FreeRTOS separada.

7. **Guardar el rècord sempre** al final de la partida (no només si és millor), protegint amb `recordMutex`:
```cpp
if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    saveRecord(score);
    xSemaphoreGive(recordMutex);
}
```

8. **El joc s'auto-registra al dashboard** — en cridar `saveRecord()` per primera vegada, el joc apareix automàticament al dashboard web sense cap configuració addicional.

### Estructura que ha de tenir `runGame()`

```cpp
void runGame() {
    // 1. Carregar rècord
    int bestScore = loadRecord();

    // 2. Inicialitzar variables del joc
    int score = 0;

    // 3. Dibuixar pantalla inicial
    tft.fillScreen(TFT_BLACK);

    // 4. Bucle principal
    unsigned long lastFrame = millis();
    while (true) {

        // Control de framerate (recomanat 20ms = ~50fps)
        if (millis() - lastFrame < 20) continue;
        lastFrame = millis();

        // 5. Llegir controls
        int rawX = analogRead(JOY_X_PIN);
        int rawY = analogRead(JOY_Y_PIN);
        bool btnA = (digitalRead(BTN_A_PIN) == LOW);

        // 6. Lògica del joc
        // ...

        // 7. Dibuixar
        tft.startWrite();
        // ...
        tft.endWrite();

        // 8. Fi de joc -> guardar rècord i tornar al menú
        if (gameOver) {
            if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                saveRecord(score);
                xSemaphoreGive(recordMutex);
            }
            // Mostrar pantalla game over, esperar botó...
            return;  // <- OBLIGATORI per tornar al menú
        }
    }
}
```

### Bones pràctiques de dibuix

- Usar `tft.startWrite()` / `tft.endWrite()` per agrupar operacions.
- **No fer `fillScreen()` cada frame** — esborrar només les zones que canvien.
- Per esborrar un sprite: `tft.fillRect(x, y, w, h, COLOR_FONS)` a la posició anterior.

### Freqüències musicals (rang recomanat: 200-600Hz)

| Nota | Freqüència |
|------|-----------|
| A3   | 220.0 Hz  |
| C4   | 261.6 Hz  |
| D4   | 293.7 Hz  |
| E4   | 329.6 Hz  |
| G4   | 392.0 Hz  |
| A4   | 440.0 Hz  |
| C5   | 523.3 Hz  |
| D5   | 587.3 Hz  |

Volum recomanat: `0.07f` música de fons, `0.10f` efectes. Mai superar `0.20f`.

---

## API de hardware

### Pantalla

```cpp
SCREEN_W = 320   // amplada en píxels
SCREEN_H = 480   // alçada en píxels

tft.fillScreen(color);
tft.fillRect(x, y, w, h, color);
tft.drawRect(x, y, w, h, color);
tft.fillCircle(x, y, r, color);
tft.drawLine(x0, y0, x1, y1, color);
tft.drawFastHLine(x, y, w, color);
tft.drawFastVLine(x, y, h, color);

tft.setTextSize(n);          // 1=petit, 2=mitjà, 3=gran, 4=molt gran
tft.setTextColor(color, bg);
tft.setCursor(x, y);
tft.print("text");
tft.printf("valor: %d", n);
int w = tft.textWidth("text");

// Colors predefinits
TFT_BLACK, TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE
TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_DARKGREY

// Color personalitzat
uint16_t color = tft.color565(r, g, b);  // r,g,b: 0-255

// Agrupar operacions (imprescindible per bon rendiment)
tft.startWrite();
// ...
tft.endWrite();
```

### Controls

```cpp
// Joystick analògic (0-4095, centre ~2048)
int rawX = analogRead(JOY_X_PIN);   // esquerra < 1748 / dreta > 2348
int rawY = analogRead(JOY_Y_PIN);   // amunt < 1748 / avall > 2348
bool joyBtn = (digitalRead(JOY_SW_PIN) == LOW);

// Botons (LOW = premut)
bool btnA = (digitalRead(BTN_A_PIN) == LOW);
bool btnB = (digitalRead(BTN_B_PIN) == LOW);

// Obtenir direcció (-1, 0, 1)
int dirX = (rawX < 1748) ? -1 : (rawX > 2348) ? 1 : 0;
int dirY = (rawY < 1748) ? -1 : (rawY > 2348) ? 1 : 0;
```

### Àudio

```cpp
// To sinusoïdal (síncron — bloqueja l'execució)
playTone(float freq, int durationMs, float volume);

// Silenci
playSilence(int durationMs);

// Exemple
playTone(440.0f, 50, 0.10f);          // La4, 50ms
playTone(261.6f, 100, 0.10f);         // C4
playSilence(20);
playTone(392.0f, 200, 0.12f);         // G4
```

### Rècords (NVS — persisteix entre reinicis i entre jocs)

```cpp
// Guardar rècord — sempre protegir amb mutex
if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    saveRecord(score);   // guarda a l'historial i actualitza el màxim si cal
    xSemaphoreGive(recordMutex);
}

// Llegir rècord màxim
int best = loadRecord();
```

L'historial guarda les **últimes 20 partides** per joc. Apareix automàticament al dashboard web.

---

## Dashboard i MCP

### Dashboard web

Accessible a `http://192.168.4.1` mentre la consola està encesa (WiFi sempre actiu).

Mostra per cada joc:
- Rècord màxim
- Nombre de partides jugades
- Mitjana de puntuació
- Darrera puntuació
- Gràfica de barres de les últimes 10 partides

Els jocs apareixen automàticament quan es guarda el primer rècord. No cal configurar res.

### Endpoints MCP

La consola exposa un servidor MCP per integrar-se amb LLMs (Ollama, etc.):

| Endpoint | Descripció |
|----------|-----------|
| `GET /mcp/tools` | Llista les tools disponibles |
| `GET /mcp/tools/call?tool=get_records` | Rècords i historial de tots els jocs |
| `GET /mcp/tools/call?tool=get_status` | Uptime, memòria lliure, IP, versió |
| `GET /mcp/tools/call?tool=get_system_info` | CPU, flash, PSRAM, SDK |

### Ús amb Ollama

```bash
# Instal·lar dependències
pip install ollama requests

# Aturar Ollama i reiniciar forçant CPU (recomanat amb GPU petita)
pkill ollama
CUDA_VISIBLE_DEVICES="" ollama serve &
sleep 3

# Executar el pont MCP
python3 espectro_mcp.py
```

Exemples de preguntes:
- "Quin és el rècord del Road Rush?"
- "Quantes partides s'han jugat en total?"
- "Quant temps porta encesa la consola?"
- "Quanta memòria lliure té l'ESP32?"

---

## Exemple mínim

```cpp
// Variables globals:
int playerX, playerY, score;

// runGame():
void runGame() {
    int bestScore = loadRecord();
    playerX = SCREEN_W / 2;
    playerY = SCREEN_H / 2;
    score = 0;

    tft.fillScreen(TFT_BLACK);

    unsigned long lastFrame = millis();
    unsigned long lastScore = millis();

    while (true) {
        if (millis() - lastFrame < 20) continue;
        lastFrame = millis();

        if (millis() - lastScore > 1000) {
            lastScore = millis();
            score++;
        }

        int rawX = analogRead(JOY_X_PIN);
        int rawY = analogRead(JOY_Y_PIN);
        int dirX = (rawX < 1748) ? -1 : (rawX > 2348) ? 1 : 0;
        int dirY = (rawY < 1748) ? -1 : (rawY > 2348) ? 1 : 0;

        tft.fillRect(playerX-10, playerY-10, 20, 20, TFT_BLACK);
        playerX = constrain(playerX + dirX * 4, 10, SCREEN_W-10);
        playerY = constrain(playerY + dirY * 4, 10, SCREEN_H-10);

        tft.startWrite();
        tft.fillRect(playerX-10, playerY-10, 20, 20, TFT_GREEN);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.printf("Pts: %d  ", score);
        tft.endWrite();

        if (digitalRead(BTN_B_PIN) == LOW) {
            if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                saveRecord(score);
                xSemaphoreGive(recordMutex);
            }
            return;
        }
    }
}
```

---

## Pujar el joc a la consola

1. Compila: `pio run`
2. El binari és a: `.pio/build/rymcu-esp32-s3-devkitc-1/firmware.bin`
3. Prem el **botó B** al menú per entrar al Game Loader
4. Connecta't a la xarxa WiFi **ESPectro** (contrasenya: **gameloader**)
5. Obre el navegador a **http://192.168.4.1**
6. Selecciona el `firmware.bin` i prem "Pujar joc"
7. La consola reinicia amb el nou joc

---

## Pins de referència

| GPIO | Funció |
|------|--------|
| 38 | SPI MOSI (pantalla) |
| 47 | SPI SCLK (pantalla) |
| 48 | SPI MISO (pantalla) |
| 2 | TFT DC |
| 1 | TFT CS |
| 0 | TFT RST |
| 39 | TFT BL (retroil·luminació) |
| 5 | Joystick VRX (eix X) |
| 4 | Joystick VRY (eix Y) |
| 42 | Joystick SW (botó) |
| 40 | Botó A |
| 41 | Botó B |
| 8 | I2S BCLK |
| 16 | I2S LRCLK |
| 18 | I2S DIN |