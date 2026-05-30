# ESPectro — Template de Joc

Template per desenvolupar jocs per a la **consola portàtil ESPectro** basada en ESP32-S3.

---

## Índex

1. [Arquitectura del sistema](#arquitectura)
2. [Requisits](#requisits)
3. [Instruccions per a humans](#humans)
4. [Instruccions per a LLMs](#llms)
5. [API de hardware](#api)
6. [Exemple mínim](#exemple)
7. [Pujar el joc a la consola](#upload)

---

## Arquitectura del sistema {#arquitectura}

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
                        ├─ Puja nou .bin via navegador
                        └─ Botó A → torna al menú
```

Cada joc és un firmware complet que inclou el launcher. Quan puges un joc nou via WiFi, sobreescriu l'anterior. Per tornar al menú des del joc, la funció `runGame()` ha de fer `return`.

---

## Requisits {#requisits}

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

## Instruccions per a humans {#humans}

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

#### 2.2 Títol del menú
```cpp
const char* linia1 = "NOM";   // TODO: primera línia del títol
const char* linia2 = "JOC";   // TODO: segona línia del títol
```
Exemple:
```cpp
const char* linia1 = "SUPER";
const char* linia2 = "SNAKE";
```

#### 2.3 Variables globals del joc
```cpp
// TODO: VARIABLES GLOBALS DEL JOC
```
Declara aquí totes les variables que necessita el teu joc.

#### 2.4 Lògica del joc dins de `runGame()`
```cpp
void runGame() {
    // TODO: implementa el teu joc aquí
}
```
Veure la secció [API de hardware](#api) per als controls i la pantalla.

### Pas 3 — Compila i puja
```
pio run --target upload
```
O via Game Loader (veure [Pujar el joc](#upload)).

---

## Instruccions per a LLMs {#llms}

> Aquesta secció explica com generar un joc complet per a la consola ESPectro a partir de la template.

### Regles obligatòries

1. **NO modificar** cap secció marcada com `// NO MODIFICAR`. Inclou: configuració de pantalla, pins, WiFi, Game Loader, menú principal, setup(), loop().

2. **Sempre fer `return`** al final de `runGame()` per tornar al menú. Mai usar `while(true)` sense condició de sortida al nivell principal.

3. **`RECORD_KEY`** ha de ser una string única per joc, en minúscules, sense espais ni caràcters especials. Exemples vàlids: `"snake"`, `"pong"`, `"tetris_v2"`.

4. **No usar `delay()` llargs** dins del bucle de joc — fa que els controls siguin irresponsables. Usar `millis()` per controlar el timing.

5. **La pantalla és de 320×480 píxels**, orientació vertical (portrait), rotació 2. L'origen (0,0) és a la cantonada superior esquerra.

6. **L'àudio és síncron** — `playTone()` bloqueja l'execució. Per sons durant el joc, usar-los amb moderació o implementar una tasca FreeRTOS separada (veure exemple al Road Rush).

### Estructura que ha de tenir `runGame()`

```cpp
void runGame() {
    // 1. Carregar rècord
    int bestScore = loadRecord();

    // 2. Inicialitzar variables del joc
    int score = 0;
    // ... altres variables

    // 3. Dibuixar pantalla inicial
    tft.fillScreen(TFT_BLACK);
    // ...

    // 4. Bucle principal
    unsigned long lastFrame = millis();
    while (true) {

        // Control de framerate (recomanat 20-50ms per frame)
        if (millis() - lastFrame < 20) continue;
        lastFrame = millis();

        // 5. Llegir controls
        // ...

        // 6. Lògica del joc
        // ...

        // 7. Dibuixar
        tft.startWrite();
        // ...
        tft.endWrite();

        // 8. Condició de fi de joc
        if (gameOver) {
            if (score > bestScore) saveRecord(score);
            // Mostrar pantalla game over
            // Esperar botó
            return;  // <- OBLIGATORI per tornar al menú
        }
    }
}
```

### Bones pràctiques de dibuix

- Usar `tft.startWrite()` / `tft.endWrite()` per agrupar operacions de dibuix i maximitzar la velocitat.
- **No fer `fillScreen()` cada frame** — és molt lent. Esborrar només les zones que canvien.
- Per esborrar un sprite: `tft.fillRect(x, y, w, h, COLOR_FONS)` a la posició anterior.

### Freqüències musicals de referència (rang recomanat: 200-600Hz)

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

Volum recomanat: `0.07f` per música de fons, `0.10f` per efectes puntuals. Mai superar `0.25f`.

---

## API de hardware {#api}

### Pantalla

```cpp
// Dimensions
SCREEN_W = 320   // amplada en píxels
SCREEN_H = 480   // alçada en píxels

// Operacions bàsiques
tft.fillScreen(color);
tft.fillRect(x, y, w, h, color);
tft.drawRect(x, y, w, h, color);
tft.fillCircle(x, y, r, color);
tft.drawLine(x0, y0, x1, y1, color);
tft.drawFastHLine(x, y, w, color);
tft.drawFastVLine(x, y, h, color);

// Text
tft.setTextSize(n);          // 1=petit, 2=mitjà, 3=gran, 4=molt gran
tft.setTextColor(color, bg); // color text i fons
tft.setCursor(x, y);
tft.print("text");
tft.printf("valor: %d", n);
int w = tft.textWidth("text"); // amplada en píxels (útil per centrar)

// Colors predefinits
TFT_BLACK, TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE
TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_DARKGREY

// Color personalitzat (RGB)
uint16_t color = tft.color565(r, g, b); // r,g,b: 0-255

// Agrupar operacions (millora rendiment)
tft.startWrite();
// ... operacions de dibuix
tft.endWrite();
```

### Controls

```cpp
// Joystick analògic (valors 0-4095, centre ~2048)
int rawX = analogRead(JOY_X_PIN);  // esquerra < 1748 / dreta > 2348
int rawY = analogRead(JOY_Y_PIN);  // amunt < 1748 / avall > 2348
bool joyBtn = (digitalRead(JOY_SW_PIN) == LOW);  // botó joystick

// Botons digitals (LOW = premut, gràcies al pull-up intern)
bool btnA = (digitalRead(BTN_A_PIN) == LOW);
bool btnB = (digitalRead(BTN_B_PIN) == LOW);

// Helper per obtenir direcció del joystick (-1, 0, 1)
// Zona morta de 300 unitats al centre
int dirX = 0;
if (rawX < 1748) dirX = -1;
else if (rawX > 2348) dirX = 1;
```

### Àudio

```cpp
// To sinusoïdal síncron
playTone(float freq, int durationMs, float volume);
// freq: freqüència en Hz
// durationMs: durada en ms
// volume: 0.0 a 0.25 (recomanat 0.07-0.15)

// Silenci
playSilence(int durationMs);

// Exemple: efecte de so curt
playTone(440.0f, 50, 0.10f);  // La4, 50ms

// Exemple: melodia
playTone(261.6f, 100, 0.10f); playSilence(20);  // C4
playTone(329.6f, 100, 0.10f); playSilence(20);  // E4
playTone(392.0f, 200, 0.12f);                   // G4
```

### Rècords (NVS — persisteix entre reinicis)

```cpp
// Guardar rècord (només escriu si és millor que l'anterior)
saveRecord(int score);

// Llegir rècord actual
int best = loadRecord();
```

---

## Exemple mínim {#exemple}

Joc on un quadre es mou per la pantalla amb el joystick i has d'aguantar el màxim de temps:

```cpp
// Al lloc de les variables globals:
int playerX, playerY;
int score;
bool gameRunning;

// runGame():
void runGame() {
    int bestScore = loadRecord();
    playerX = SCREEN_W / 2;
    playerY = SCREEN_H / 2;
    score = 0;
    gameRunning = true;

    tft.fillScreen(TFT_BLACK);

    unsigned long lastFrame = millis();
    unsigned long lastScore = millis();

    while (true) {
        if (millis() - lastFrame < 20) continue;
        lastFrame = millis();

        // Puntuació per temps
        if (millis() - lastScore > 1000) {
            lastScore = millis();
            score++;
        }

        // Controls
        int rawX = analogRead(JOY_X_PIN);
        int rawY = analogRead(JOY_Y_PIN);
        int dirX = (rawX < 1748) ? -1 : (rawX > 2348) ? 1 : 0;
        int dirY = (rawY < 1748) ? -1 : (rawY > 2348) ? 1 : 0;

        // Esborrar posició anterior
        tft.fillRect(playerX-10, playerY-10, 20, 20, TFT_BLACK);

        // Moure jugador
        playerX = constrain(playerX + dirX * 4, 10, SCREEN_W-10);
        playerY = constrain(playerY + dirY * 4, 10, SCREEN_H-10);

        // Dibuixar
        tft.startWrite();
        tft.fillRect(playerX-10, playerY-10, 20, 20, TFT_GREEN);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.printf("Pts: %d  ", score);
        tft.endWrite();

        // Sortir amb botó B
        if (digitalRead(BTN_B_PIN) == LOW) {
            if (score > bestScore) saveRecord(score);
            return;
        }
    }
}
```

---

## Pujar el joc a la consola {#upload}

1. Compila el projecte: `pio run`
2. El fitxer generat és: `.pio/build/rymcu-esp32-s3-devkitc-1/firmware.bin`
3. A la consola, prem el **botó B** al menú per entrar al Game Loader
4. Connecta't a la xarxa WiFi **Consola-ESP32** (contrasenya: **gameloader**)
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
| 39 | TFT BL (retroiluminació) |
| 5 | Joystick VRX (eix X) |
| 4 | Joystick VRY (eix Y) |
| 42 | Joystick SW (botó) |
| 40 | Botó A |
| 41 | Botó B |
| 8 | I2S BCLK |
| 16 | I2S LRCLK |
| 18 | I2S DIN |