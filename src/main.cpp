// ============================================================
//  LAUNCHER — Consola ESP32-S3
//  Arranca siempre primero. Si el botón B está pulsado al
//  encender, entra en modo Game Loader (WiFi AP + web upload).
//  Si no, arranca el juego instalado en ota_1.
// ============================================================

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// ── Pines ────────────────────────────────────────────────────
#define PIN_BTN_B    41   // botón para entrar en Game Loader
#define PIN_TFT_CS   10
#define PIN_TFT_DC    7
#define PIN_TFT_RST   6
#define PIN_TFT_BL   15
#define PIN_MOSI     11
#define PIN_MISO     47
#define PIN_SCLK     12

// ── WiFi AP ──────────────────────────────────────────────────
#define AP_SSID   "Consola-ESP32"
#define AP_PASS   "gameloader"   // mínimo 8 caracteres, o "" para abierta
#define AP_IP     "192.168.4.1"

// ── Pantalla ─────────────────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
    lgfx::Bus_SPI       _bus;
    lgfx::Panel_ILI9488 _panel;
    lgfx::Light_PWM     _light;
public:
    LGFX() {
        { auto cfg = _bus.config();
          cfg.spi_host   = SPI2_HOST;
          cfg.spi_mode   = 0;
          cfg.freq_write = 40000000;
          cfg.pin_sclk   = PIN_SCLK;
          cfg.pin_mosi   = PIN_MOSI;
          cfg.pin_miso   = PIN_MISO;
          cfg.pin_dc     = PIN_TFT_DC;
          _bus.config(cfg);
          _panel.setBus(&_bus); }

        { auto cfg = _panel.config();
          cfg.pin_cs   = PIN_TFT_CS;
          cfg.pin_rst  = PIN_TFT_RST;
          cfg.pin_busy = -1;
          cfg.memory_width  = 320;
          cfg.memory_height = 480;
          cfg.panel_width   = 320;
          cfg.panel_height  = 480;
          cfg.invert    = false;
          cfg.rgb_order = false;
          _panel.config(cfg); }

        { auto cfg = _light.config();
          cfg.pin_bl      = PIN_TFT_BL;
          cfg.invert      = false;
          cfg.freq        = 44100;
          cfg.pwm_channel = 0;
          _light.config(cfg);
          _panel.setLight(&_light); }

        setPanel(&_panel);
    }
};

LGFX          tft;
WebServer     server(80);
Preferences   prefs;

// ── Página web de carga ───────────────────────────────────────
const char PAGE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Consola ESP32 — Game Loader</title>
<style>
  body { background:#111; color:#eee; font-family:monospace;
         display:flex; flex-direction:column; align-items:center;
         justify-content:center; min-height:100vh; margin:0; }
  h1   { color:#0f0; font-size:1.4em; margin-bottom:0.2em; }
  p    { color:#aaa; margin:0.3em 0 1.2em; }
  .box { background:#1a1a1a; border:1px solid #333; border-radius:12px;
         padding:2em; max-width:400px; width:90%; text-align:center; }
  input[type=file] { display:none; }
  label.btn, button {
    display:inline-block; padding:0.7em 1.6em; margin:0.4em;
    background:#0a0; color:#fff; border:none; border-radius:8px;
    font-size:1em; font-family:monospace; cursor:pointer; }
  label.btn:hover, button:hover { background:#0c0; }
  #filename { color:#0f0; margin:0.5em 0; min-height:1.2em; }
  #progress { width:100%; background:#222; border-radius:6px;
              height:18px; margin:1em 0; display:none; }
  #bar      { height:100%; width:0; background:#0a0;
              border-radius:6px; transition:width 0.3s; }
  #status   { color:#ff0; min-height:1.5em; }
  .ok  { color:#0f0 !important; }
  .err { color:#f00 !important; }
</style>
</head>
<body>
<div class="box">
  <h1>🎮 Game Loader</h1>
  <p>Sube el .bin compilado con PlatformIO</p>

  <label class="btn" for="file">📂 Elegir .bin</label>
  <input type="file" id="file" accept=".bin">
  <div id="filename">Ningún archivo seleccionado</div>

  <button onclick="upload()">⬆ Subir juego</button>

  <div id="progress"><div id="bar"></div></div>
  <div id="status"></div>
</div>

<script>
const fileInput = document.getElementById('file');
const filename  = document.getElementById('filename');
const status    = document.getElementById('status');
const progress  = document.getElementById('progress');
const bar       = document.getElementById('bar');

fileInput.addEventListener('change', () => {
  filename.textContent = fileInput.files[0]?.name || 'Ningún archivo';
});

function upload() {
  const file = fileInput.files[0];
  if (!file) { status.textContent = 'Selecciona un archivo primero'; return; }
  if (!file.name.endsWith('.bin')) { status.textContent = 'Debe ser un .bin'; return; }

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update', true);

  xhr.upload.onprogress = e => {
    if (e.lengthComputable) {
      const pct = Math.round(e.loaded / e.total * 100);
      progress.style.display = 'block';
      bar.style.width = pct + '%';
      status.textContent = 'Subiendo... ' + pct + '%';
      status.className = '';
    }
  };

  xhr.onload = () => {
    if (xhr.status === 200) {
      status.textContent = '✅ Juego instalado. Reiniciando...';
      status.className = 'ok';
      setTimeout(() => { status.textContent = 'Consola reiniciada. Puedes cerrar esto.'; }, 4000);
    } else {
      status.textContent = '❌ Error: ' + xhr.responseText;
      status.className = 'err';
    }
  };

  xhr.onerror = () => {
    status.textContent = '❌ Error de conexión';
    status.className = 'err';
  };

  const fd = new FormData();
  fd.append('firmware', file, file.name);
  xhr.send(fd);
}
</script>
</body>
</html>
)rawhtml";

// ── Handlers web ──────────────────────────────────────────────
void handleRoot() {
    server.send_P(200, "text/html", PAGE_HTML);
}

void handleUpdate() {
    server.send(200, "text/plain",
                Update.hasError() ? "FALLO" : "OK");
    delay(500);
    ESP.restart();
}

void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Recibiendo: %s\n", upload.filename.c_str());
        tft.fillRect(0, 260, 320, 30, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setCursor(10, 265);
        tft.print("Recibiendo firmware...");

        // Escribir en el slot OTA que NO está activo
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
        }

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize)
                != upload.currentSize) {
            Update.printError(Serial);
        }
        // Barra de progreso en pantalla
        static size_t totalRecibido = 0;
        totalRecibido += upload.currentSize;
        int pct = (int)(totalRecibido / 10000);  // aprox
        if (pct > 100) pct = 100;
        tft.fillRect(10, 295, pct * 3, 10, TFT_GREEN);

    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("Instalado: %u bytes\n", upload.totalSize);
            tft.fillRect(0, 260, 320, 60, TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 265);
            tft.print("Juego instalado!");
            tft.setCursor(10, 285);
            tft.print("Reiniciando...");
        } else {
            Update.printError(Serial);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(10, 265);
            tft.print("Error instalando");
        }
    }
}

// ── Pantalla Game Loader ──────────────────────────────────────
void drawLoaderScreen() {
    tft.fillScreen(TFT_BLACK);

    // Título
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(30, 40);
    tft.print("GAME LOADER");

    // Separador
    tft.drawFastHLine(10, 90, 300, TFT_DARKGREY);

    // Info WiFi
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 110);
    tft.print("Red WiFi:");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 132);
    tft.print(AP_SSID);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 162);
    tft.print("Contrasena:");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 184);
    tft.print(AP_PASS);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 214);
    tft.print("Abre en el navegador:");
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 236);
    tft.printf("http://%s", AP_IP);

    // Separador
    tft.drawFastHLine(10, 258, 300, TFT_DARKGREY);

    // Zona progreso
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 268);
    tft.print("Esperando juego...");

    // Instruccion salir
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(10, 450);
    tft.print("Reinicia para salir sin instalar");
}

// ── Pantalla sin juego ────────────────────────────────────────
void drawNoGameScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(40, 180);
    tft.print("Sin juego instalado");
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(20, 230);
    tft.print("Mantén botón B al arrancar");
    tft.setCursor(20, 248);
    tft.print("para entrar en Game Loader");
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Botón B
    pinMode(PIN_BTN_B, INPUT_PULLUP);

    // Pantalla
    tft.init();
    tft.setRotation(0);
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);

    // Leer botón B al arrancar
    bool modoLoader = (digitalRead(PIN_BTN_B) == LOW);

    if (modoLoader) {
        // ── MODO GAME LOADER ──────────────────────────────────
        Serial.println("Modo Game Loader");

        // Arrancar AP
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASS);
        WiFi.softAPConfig(
            IPAddress(192,168,4,1),
            IPAddress(192,168,4,1),
            IPAddress(255,255,255,0)
        );

        // Rutas web
        server.on("/",        HTTP_GET,  handleRoot);
        server.on("/update",  HTTP_POST, handleUpdate, handleUpdateUpload);
        server.begin();

        drawLoaderScreen();
        Serial.printf("AP: %s  IP: %s\n", AP_SSID, AP_IP);

        // Loop del loader
        while (true) {
            server.handleClient();
        }

    } else {
        // ── MODO NORMAL: arrancar juego instalado ─────────────
        // Comprobar si hay un juego instalado en ota_1
        const esp_partition_t* ota1 =
            esp_partition_find_first(
                ESP_PARTITION_TYPE_APP,
                ESP_PARTITION_SUBTYPE_APP_OTA_1,
                nullptr);

        esp_ota_img_states_t state;
        bool hayJuego = false;

        if (ota1 != nullptr) {
            if (esp_ota_get_state_partition(ota1, &state) == ESP_OK) {
                hayJuego = (state == ESP_OTA_IMG_VALID ||
                            state == ESP_OTA_IMG_UNDEFINED);
            }
        }

        if (hayJuego) {
            // Activar la partición ota_1 y reiniciar en ella
            esp_ota_set_boot_partition(ota1);
            Serial.println("Arrancando juego...");

            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(60, 220);
            tft.print("Cargando...");
            delay(500);

            esp_restart();
        } else {
            // No hay juego instalado
            drawNoGameScreen();
            Serial.println("Sin juego instalado");
        }
    }
}

void loop() {
    // El launcher no usa loop — todo ocurre en setup()
    // Si llegamos aquí es porque no hay juego y esperamos
    delay(1000);
}