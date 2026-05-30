// ============================================================
//  TEMPLATE JOC — Consola ESP32-S3
//  Copia aquest fitxer i omple les seccions marcades amb TODO
//
//  INSTRUCCIONS:
//  1. Copia aquest fitxer al teu projecte PlatformIO
//  2. Omple les seccions marcades amb TODO
//  3. Compila i puja el .bin via Game Loader
//
//  NOTES IMPORTANTS:
//  - NO canviïs la configuració de pantalla, pins ni WiFi
//  - El menú principal i el Game Loader ja estan integrats
//  - Usa saveRecord()/loadRecord() per guardar el rècord
//  - El joc ha de tenir un punt de sortida (return a runGame())
//    per tornar al menú quan l'usuari perdi o surti
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <driver/i2s.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <nvs_flash.h>
#include <nvs.h>

// ============================================================
//  PANTALLA — NO MODIFICAR
// ============================================================
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
          cfg.pin_sclk   = 47;
          cfg.pin_mosi   = 38;
          cfg.pin_miso   = 48;
          cfg.pin_dc     = 2;
          _bus.config(cfg);
          _panel.setBus(&_bus); }
        { auto cfg = _panel.config();
          cfg.pin_cs   = 1;
          cfg.pin_rst  = 0;
          cfg.pin_busy = -1;
          cfg.memory_width  = 320;
          cfg.memory_height = 480;
          cfg.panel_width   = 320;
          cfg.panel_height  = 480;
          cfg.invert    = false;
          cfg.rgb_order = false;
          _panel.config(cfg); }
        { auto cfg = _light.config();
          cfg.pin_bl      = 39;
          cfg.invert      = false;
          cfg.freq        = 44100;
          cfg.pwm_channel = 0;
          _light.config(cfg);
          _panel.setLight(&_light); }
        setPanel(&_panel);
    }
};

LGFX tft;

// ============================================================
//  PINS — NO MODIFICAR
// ============================================================
#define JOY_X_PIN  5    // ADC eix X joystick
#define JOY_Y_PIN  4    // ADC eix Y joystick
#define JOY_SW_PIN 42   // Botó joystick (premut = LOW)
#define BTN_A_PIN  40   // Botó A (premut = LOW)
#define BTN_B_PIN  41   // Botó B (premut = LOW)

// Resolució pantalla
#define SCREEN_W 320
#define SCREEN_H 480

// ============================================================
//  I2S AUDIO — NO MODIFICAR
// ============================================================
#define I2S_BCLK    8
#define I2S_LRCLK  16
#define I2S_DIN    18
#define SAMPLE_RATE 44100
#define I2S_PORT    I2S_NUM_0

void audioInit() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 512,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
    };
    i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_BCLK,
        .ws_io_num    = I2S_LRCLK,
        .data_out_num = I2S_DIN,
        .data_in_num  = I2S_PIN_NO_CHANGE,
    };
    i2s_set_pin(I2S_PORT, &pins);
    i2s_zero_dma_buffer(I2S_PORT);
}

// Genera un to sinusoïdal
// freq: freqüència en Hz (ex: 440.0 = La4)
// durationMs: durada en mil·lisegons
// volume: 0.0 a 1.0 (recomanat < 0.2 per evitar saturació)
void playTone(float freq, int durationMs, float volume = 0.1f) {
    const int bufSize = 256;
    int16_t buf[bufSize * 2];
    const int samples = SAMPLE_RATE * durationMs / 1000;
    int written = 0;
    while (written < samples) {
        int chunk = min(bufSize, samples - written);
        for (int i = 0; i < chunk; i++) {
            float t   = (float)(written + i) / SAMPLE_RATE;
            int16_t v = (int16_t)(sinf(2.0f * M_PI * freq * t) * 32767.0f * volume);
            buf[i*2] = v; buf[i*2+1] = v;
        }
        size_t bw;
        i2s_write(I2S_PORT, buf, chunk * 4, &bw, portMAX_DELAY);
        written += chunk;
    }
}

void playSilence(int durationMs) {
    int16_t buf[512] = {0};
    const int samples = SAMPLE_RATE * durationMs / 1000;
    int written = 0;
    while (written < samples) {
        int chunk = min(256, samples - written);
        size_t bw;
        i2s_write(I2S_PORT, buf, chunk * 4, &bw, portMAX_DELAY);
        written += chunk;
    }
}

// ============================================================
//  SPLASH SCREEN — NO MODIFICAR
// ============================================================
#define SCREEN_W 320
#define SCREEN_H 480

void showSplash() {
    tft.fillScreen(TFT_BLACK);
    const uint16_t TARONJA = tft.color565(255, 80, 0);
    tft.fillRect(0, 0, SCREEN_W, 6, TARONJA);
    tft.setTextSize(5);
    int y_titol = 140;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(SCREEN_W/2 - tft.textWidth("ESPectro")/2, y_titol);
    tft.print("ESP");
    tft.setTextColor(TARONJA, TFT_BLACK);
    tft.print("ectro");
    tft.drawFastHLine(40, y_titol+55, SCREEN_W-80, TARONJA);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    const char* slogan = "Consola portatil ESP32-S3";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(slogan)/2, y_titol+70);
    tft.print(slogan);
    const char* autors = "Noel Medina & Bernat Figuerola - UPC 2026";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(autors)/2, y_titol+86);
    tft.print(autors);
    tft.fillRect(0, SCREEN_H-6, SCREEN_W, 6, TARONJA);
    playTone(220.0f, 120, 0.15f); playSilence(30);
    playTone(277.2f, 120, 0.15f); playSilence(30);
    playTone(329.6f, 120, 0.15f);
    delay(500);
}

// ============================================================
//  RECORDS NVS — NO MODIFICAR
// ============================================================
// Clau del joc — TODO: canvia "nom_joc" pel nom del teu joc
// ex: "snake", "pong", "tetris"
#define RECORD_KEY "nom_joc"  // TODO: canvia això!

int loadRecord() {
    nvs_handle_t h;
    nvs_flash_init();
    int32_t r = 0;
    if (nvs_open("records", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, RECORD_KEY, &r);
        nvs_close(h);
    }
    return (int)r;
}

void saveRecord(int score) {
    nvs_handle_t h;
    nvs_flash_init();
    if (nvs_open("records", NVS_READWRITE, &h) == ESP_OK) {
        int32_t current = 0;
        nvs_get_i32(h, RECORD_KEY, &current);
        if (score > current) {
            nvs_set_i32(h, RECORD_KEY, (int32_t)score);
            nvs_commit(h);
        }
        nvs_close(h);
    }
}

String getAllRecords() {
    nvs_handle_t h;
    nvs_flash_init();
    String json = "{";
    if (nvs_open("records", NVS_READONLY, &h) == ESP_OK) {
        // TODO: afegeix aquí totes les claus de rècords que vulguis mostrar
        int32_t val = 0;
        nvs_get_i32(h, RECORD_KEY, &val);
        json += "\"" + String(RECORD_KEY) + "\":" + String(val);
        nvs_close(h);
    }
    json += "}";
    return json;
}

// ============================================================
//  WIFI / GAME LOADER — NO MODIFICAR
// ============================================================
#define AP_SSID "ESPectro"
#define AP_PASS "gameloader"
#define AP_IP   "192.168.4.1"

WebServer server(80);

const char PAGE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="ca">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Game Loader</title>
<style>
  body{background:#111;color:#eee;font-family:monospace;display:flex;
       flex-direction:column;align-items:center;justify-content:center;
       min-height:100vh;margin:0;padding:1em;}
  h1{color:#0f0;}
  .box{background:#1a1a1a;border:1px solid #333;border-radius:12px;
       padding:2em;max-width:420px;width:100%;}
  input[type=file]{display:none;}
  label.btn,button{display:inline-block;padding:0.7em 1.4em;margin:0.3em;
    background:#0a0;color:#fff;border:none;border-radius:8px;
    font-size:1em;font-family:monospace;cursor:pointer;}
  #filename{color:#0f0;margin:0.5em 0;}
  #progress{width:100%;background:#222;border-radius:6px;height:18px;
            margin:1em 0;display:none;}
  #bar{height:100%;width:0;background:#0a0;border-radius:6px;transition:width 0.3s;}
  #status{color:#ff0;min-height:1.5em;}
  .ok{color:#0f0!important;} .err{color:#f00!important;}
  .records{background:#111;border:1px solid #333;border-radius:8px;
           padding:1em;margin-top:1.5em;}
  .records h3{color:#ff0;margin:0 0 0.5em;}
  .rec-row{display:flex;justify-content:space-between;
           border-bottom:1px solid #222;padding:0.3em 0;}
  .rec-name{color:#aaa;} .rec-val{color:#0f0;font-weight:bold;}
</style>
</head>
<body>
<div class="box">
  <h1>🎮 Game Loader</h1>
  <p>Puja el .bin compilat amb PlatformIO</p>
  <label class="btn" for="file">📂 Triar .bin</label>
  <input type="file" id="file" accept=".bin">
  <div id="filename">Cap arxiu seleccionat</div>
  <button onclick="upload()">⬆ Pujar joc</button>
  <div id="progress"><div id="bar"></div></div>
  <div id="status"></div>
  <div class="records">
    <h3>🏆 Rècords</h3>
    <div id="recs">Carregant...</div>
  </div>
</div>
<script>
const fi=document.getElementById('file');
fi.addEventListener('change',()=>{
  document.getElementById('filename').textContent=fi.files[0]?.name||'Cap arxiu';
});
fetch('/records').then(r=>r.json()).then(data=>{
  const d=document.getElementById('recs');
  const e=Object.entries(data);
  if(!e.length){d.innerHTML='<span style="color:#555">Cap rècord</span>';return;}
  d.innerHTML=e.map(([k,v])=>
    `<div class="rec-row">
       <span class="rec-name">${k.replace(/_/g,' ').toUpperCase()}</span>
       <span class="rec-val">${v} pts</span>
     </div>`).join('');
}).catch(()=>{document.getElementById('recs').textContent='Error';});
function upload(){
  const file=fi.files[0];
  const status=document.getElementById('status');
  const bar=document.getElementById('bar');
  const prog=document.getElementById('progress');
  if(!file){status.textContent='Selecciona un arxiu';return;}
  if(!file.name.endsWith('.bin')){status.textContent='Ha de ser .bin';return;}
  const xhr=new XMLHttpRequest();
  xhr.open('POST','/update',true);
  xhr.upload.onprogress=e=>{
    if(e.lengthComputable){
      const pct=Math.round(e.loaded/e.total*100);
      prog.style.display='block';
      bar.style.width=pct+'%';
      status.textContent='Pujant... '+pct+'%';
    }
  };
  xhr.onload=()=>{
    if(xhr.status===200){status.textContent='✅ Joc instal·lat. Reiniciant...';status.className='ok';}
    else{status.textContent='❌ Error: '+xhr.responseText;status.className='err';}
  };
  const fd=new FormData();
  fd.append('firmware',file,file.name);
  xhr.send(fd);
}
</script>
</body>
</html>
)rawhtml";

void handleRoot()    { server.send_P(200, "text/html", PAGE_HTML); }
void handleRecords() { server.send(200, "application/json", getAllRecords()); }
void handleUpdate()  {
    server.send(200, "text/plain", Update.hasError() ? "FALLO" : "OK");
    delay(500);
    ESP.restart();
}
void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        tft.fillRect(0,260,320,60,TFT_BLACK);
        tft.setTextColor(TFT_YELLOW,TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10,270); tft.print("Rebent firmware...");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
            Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
        static size_t total = 0;
        total += upload.currentSize;
        tft.fillRect(10,300,constrain((int)(total/10000),0,100)*3,10,TFT_GREEN);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            tft.fillRect(0,260,320,60,TFT_BLACK);
            tft.setTextColor(TFT_GREEN,TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10,270); tft.print("Joc instal·lat!");
            tft.setCursor(10,295); tft.print("Reiniciant...");
        } else { Update.printError(Serial); }
    }
}

void runGameLoader() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(30, 40);   tft.print("GAME LOADER");
    tft.drawFastHLine(10, 88, 300, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 108);  tft.print("Xarxa WiFi:");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 130);  tft.print(AP_SSID);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 158);  tft.print("Contrasenya:");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 180);  tft.print(AP_PASS);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 210);  tft.print("Obre al navegador:");
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(10, 232);  tft.printf("http://%s", AP_IP);
    tft.drawFastHLine(10, 256, 300, TFT_DARKGREY);
    tft.setTextColor(tft.color565(150,150,150), TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 440);  tft.print("Prem A per tornar al menu");

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.softAPConfig(
        IPAddress(192,168,4,1),
        IPAddress(192,168,4,1),
        IPAddress(255,255,255,0)
    );
    server.on("/",        HTTP_GET,  handleRoot);
    server.on("/records", HTTP_GET,  handleRecords);
    server.on("/update",  HTTP_POST, handleUpdate, handleUpdateUpload);
    server.begin();

    while (true) {
        server.handleClient();
        if (digitalRead(BTN_A_PIN) == LOW) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(60, 220); tft.print("Tornant...");
            delay(500);
            server.stop();
            WiFi.mode(WIFI_OFF);
            ESP.restart();
        }
        delay(10);
    }
}

// ============================================================
//  MENÚ PRINCIPAL — NO MODIFICAR (pots canviar el títol)
// ============================================================
void drawMenu(int bestScore) {
    tft.fillScreen(TFT_BLACK);

    // TODO: canvia el títol del teu joc
    tft.setTextColor(tft.color565(255,60,0), TFT_BLACK);
    tft.setTextSize(4);
    const char* linia1 = "NOM";   // TODO: primera línia del títol
    const char* linia2 = "JOC";   // TODO: segona línia del títol
    tft.setCursor(SCREEN_W/2 - tft.textWidth(linia1)/2, 50);
    tft.print(linia1);
    tft.setCursor(SCREEN_W/2 - tft.textWidth(linia2)/2, 100);
    tft.print(linia2);

    tft.drawFastHLine(40, 158, 240, tft.color565(255,60,0));

    tft.setTextColor(tft.color565(255,215,0), TFT_BLACK);
    tft.setTextSize(2);
    String best = "Record: " + String(bestScore) + " pts";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(best)/2, 175);
    tft.print(best);

    tft.drawFastHLine(40, 210, 240, tft.color565(255,60,0));

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(SCREEN_W/2 - tft.textWidth("Prem A per jugar")/2, 250);
    tft.print("Prem A per jugar");

    tft.setCursor(SCREEN_W/2 - tft.textWidth("Prem B per carregar")/2, 290);
    tft.print("Prem B per carregar");
    tft.setCursor(SCREEN_W/2 - tft.textWidth("un nou joc")/2, 312);
    tft.print("un nou joc");
}

// ============================================================
//  TODO: VARIABLES GLOBALS DEL JOC
//  Declara aquí les variables que necessita el teu joc
// ============================================================
// exemple:
// int score = 0;
// int playerX = SCREEN_W / 2;
// bool gameRunning = true;

// ============================================================
//  TODO: FUNCIONS DEL JOC
//  Implementa aquí la lògica del teu joc
// ============================================================

// TODO: inicialitza i executa el joc
// Aquesta funció ha de fer return quan el jugador perdi o vulgui sortir
void runGame() {
    int bestScore = loadRecord();

    // TODO: inicialitza el teu joc aquí
    // exemple:
    // score = 0;
    // tft.fillScreen(TFT_BLACK);

    // TODO: bucle principal del joc
    while (true) {

        // TODO: llegir controls
        // int rawX = analogRead(JOY_X_PIN);  // 0-4095, centre ~2048
        // int rawY = analogRead(JOY_Y_PIN);  // 0-4095, centre ~2048
        // bool btnA = (digitalRead(BTN_A_PIN) == LOW);
        // bool btnB = (digitalRead(BTN_B_PIN) == LOW);
        // bool joyBtn = (digitalRead(JOY_SW_PIN) == LOW);

        // TODO: lògica del joc

        // TODO: dibuixar
        // tft.startWrite();
        // ...
        // tft.endWrite();

        // TODO: quan el joc acaba, guarda el rècord i surt
        // if (gameOver) {
        //     if (score > bestScore) saveRecord(score);
        //     return;  // <- torna al menú principal
        // }

        // Exemple de sortida amb botó B
        if (digitalRead(BTN_B_PIN) == LOW) {
            return;  // torna al menú
        }

        delay(20);  // ~50fps
    }
}

// ============================================================
//  SETUP — NO MODIFICAR
// ============================================================
void setup() {
    Serial.begin(115200);
    pinMode(JOY_X_PIN,  INPUT);
    pinMode(JOY_Y_PIN,  INPUT);
    pinMode(JOY_SW_PIN, INPUT_PULLUP);
    pinMode(BTN_A_PIN,  INPUT_PULLUP);
    pinMode(BTN_B_PIN,  INPUT_PULLUP);

    tft.init();
    tft.setRotation(2);
    tft.setBrightness(255);

    audioInit();
    // Si el teu joc usa música en tasca FreeRTOS, descomenta:
    // xTaskCreatePinnedToCore(musicTask, "music", 4096, NULL, 2, NULL, 0);

    showSplash();
}

// ============================================================
//  LOOP — NO MODIFICAR
// ============================================================
void loop() {
    int best = loadRecord();
    drawMenu(best);

    while (true) {
        if (digitalRead(BTN_A_PIN) == LOW) {
            delay(50);
            runGame();
            break;
        }
        if (digitalRead(BTN_B_PIN) == LOW) {
            delay(50);
            runGameLoader();
        }
        delay(20);
    }
}