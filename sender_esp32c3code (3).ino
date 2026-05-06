// ============================================================
//  SENDER - ESP32-C3 Super Mini
//  Industrial Energy Monitor | ESP-NOW + Web Config
//
//  WIRING SUMMARY:
//  OLED VCC  → ESP32 3V3 pin  (NOT 5V!)
//  OLED GND  → GND
//  OLED D0   → GPIO 4  (CLK)
//  OLED D1   → GPIO 5  (MOSI)
//  OLED RES  → ESP32 3V3 pin  (tie high, RST=-1 in code)
//  OLED DC   → GPIO 3
//  OLED CS   → GPIO 7
//  PZEM VCC  → 5V external supply
//  PZEM GND  → GND (shared with ESP32 GND)
//  PZEM TX   → GPIO 20 (ESP32 RX)
//  PZEM RX   → GPIO 21 (ESP32 TX)
//  BTN Reset → GPIO 10 → GND  (hold 3s to reset energy)
//  BTN Config→ GPIO 2  → GND  (hold 5s for WiFi config)
//  ESP32 VIN → 5V external supply
// ============================================================

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PZEM004Tv30.h>

// ---------- Pin Definitions ----------
#define OLED_MOSI        5
#define OLED_CLK         4
#define OLED_DC          3
#define OLED_CS          7
#define OLED_RST         -1    // RES pin tied to 3.3V on hardware
#define ENERGY_RESET_BTN 10
#define WIFI_CONFIG_BTN  2

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64

// ---------- Objects ----------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         OLED_MOSI, OLED_CLK, OLED_DC,
                         OLED_RST, OLED_CS);

WebServer server(80);
HardwareSerial pzemSerial(1);
PZEM004Tv30 pzem(pzemSerial, 20, 21);  // RX=20, TX=21

// ---------- Data Structure ----------
typedef struct struct_message {
  float voltage;
  float power;
  float energy;
} struct_message;

struct_message myData;
uint8_t broadcastAddress[6];
bool configMode = false;
float baseEnergy = 0.0;

// ---------- Save energy base to flash ----------
void saveEnergy(float val) {
  File f = LittleFS.open("/energy.txt", "w");
  if (f) { f.print(val, 4); f.close(); }
}

// ---------- Load MAC from flash ----------
void loadConfig() {
  if (LittleFS.exists("/mac.txt")) {
    File f = LittleFS.open("/mac.txt", "r");
    String macStr = f.readStringUntil('\n');
    macStr.trim();
    f.close();
    int parsed = sscanf(macStr.c_str(),
      "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
      &broadcastAddress[0], &broadcastAddress[1],
      &broadcastAddress[2], &broadcastAddress[3],
      &broadcastAddress[4], &broadcastAddress[5]);
    if (parsed != 6) {
      // Corrupted file — fall back to default
      Serial.println("MAC parse failed, using default");
      goto useDefault;
    }
    Serial.printf("Loaded MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
      broadcastAddress[0], broadcastAddress[1], broadcastAddress[2],
      broadcastAddress[3], broadcastAddress[4], broadcastAddress[5]);
    return;
  }
  useDefault:
  {
    uint8_t defaultMac[] = {0x68, 0xFE, 0x71, 0xF7, 0xF7, 0x4C};
    memcpy(broadcastAddress, defaultMac, 6);
    Serial.println("Using default MAC");
  }
}

// ---------- Web Config UI ----------
void handleRoot() {
  char macStr[18];
  snprintf(macStr, sizeof(macStr),
    "%02X:%02X:%02X:%02X:%02X:%02X",
    broadcastAddress[0], broadcastAddress[1],
    broadcastAddress[2], broadcastAddress[3],
    broadcastAddress[4], broadcastAddress[5]);

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Energy Monitor Config</title>";
  html += "<style>body{font-family:sans-serif;text-align:center;padding:30px;background:#1a1a2e;color:#eee;}";
  html += "input{padding:10px;width:220px;border-radius:6px;border:1px solid #555;background:#333;color:#eee;font-size:15px;}";
  html += "button{padding:12px 28px;background:#27ae60;color:white;border:none;border-radius:6px;font-size:15px;cursor:pointer;margin-top:14px;}";
  html += "h2{color:#27ae60;}p{color:#aaa;font-size:13px;}</style></head><body>";
  html += "<h2>Energy Monitor Config</h2>";
  html += "<p>Current MAC: <b>" + String(macStr) + "</b></p>";
  html += "<form action='/save' method='POST'>";
  html += "<p>Receiver MAC Address:</p>";
  html += "<input type='text' name='mac' placeholder='68:FE:71:F7:F7:4C' maxlength='17'><br>";
  html += "<button type='submit'>Save &amp; Restart</button>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  if (!server.hasArg("mac")) {
    server.send(400, "text/plain", "Missing MAC");
    return;
  }
  String newMac = server.arg("mac");
  newMac.trim();

  // Basic validation: must be 17 chars like XX:XX:XX:XX:XX:XX
  if (newMac.length() != 17) {
    server.send(400, "text/html",
      "<html><body style='font-family:sans-serif;text-align:center;padding:30px;'>"
      "<h3 style='color:red'>Invalid MAC format!</h3>"
      "<p>Use format: 68:FE:71:F7:F7:4C</p>"
      "<a href='/'>Go back</a></body></html>");
    return;
  }

  File f = LittleFS.open("/mac.txt", "w");
  f.print(newMac);
  f.close();
  server.send(200, "text/html",
    "<html><body style='font-family:sans-serif;text-align:center;padding:30px;background:#1a1a2e;color:#eee;'>"
    "<h3 style='color:#27ae60'>Saved! Restarting...</h3></body></html>");
  delay(1500);
  ESP.restart();
}

// ---------- Config Mode ----------
void startConfigMode() {
  configMode = true;

  // Switch to AP mode (drop ESP-NOW first)
  esp_now_deinit();
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ENERGY_MONITOR_CONFIG", "12345678");

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);  display.println("== CONFIG MODE ==");
  display.setCursor(0, 12); display.println("WiFi: ENERGY_MONITOR");
  display.setCursor(0, 22); display.println("      _CONFIG");
  display.setCursor(0, 34); display.println("Pass: 12345678");
  display.setCursor(0, 46); display.println("Open: 192.168.4.1");
  display.display();

  Serial.println("Config mode started. AP: ENERGY_MONITOR_CONFIG");
  Serial.println("IP: 192.168.4.1");
}

// ---------- ESP-NOW Send Callback (ESP32 core 3.x uses wifi_tx_info_t) ----------
void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "[ESP-NOW] Sent OK" : "[ESP-NOW] Send FAIL");
}

// ---------- Init ESP-NOW ----------
void initEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("ESP-NOW FAIL!");
    display.println("Restarting...");
    display.display();
    delay(2000);
    ESP.restart();
  }
  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
  }
}

// ---------- OLED startup splash ----------
void showSplash() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(10, 10); display.println("Energy Monitor");
  display.setCursor(10, 24); display.println("Initializing...");
  display.setCursor(10, 40); display.println("v2.0 ESP32-C3");
  display.display();
  delay(1500);
}

// ---------- Display readings ----------
void showReadings() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Row 1: Voltage
  display.setTextSize(1);
  display.setCursor(0, 0);  display.print("Voltage:");
  display.setTextSize(2);
  display.setCursor(0, 9);  display.printf("%.1f V", myData.voltage);

  // Row 2: Power
  display.setTextSize(1);
  display.setCursor(0, 30); display.print("Power:");
  display.setTextSize(2);
  display.setCursor(0, 39); display.printf("%.0f W", myData.power);

  // Row 3: Energy (small size to fit kWh)
  display.setTextSize(1);
  display.setCursor(70, 30); display.print("Energy:");
  display.setCursor(70, 42); display.printf("%.3f", myData.energy);
  display.setCursor(70, 54); display.print("kWh");

  display.display();
}

// ---------- Display PZEM not found ----------
void showNoSensor() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10); display.println("PZEM not found!");
  display.setCursor(0, 24); display.println("Check wiring:");
  display.setCursor(0, 34); display.println("TX->GPIO20");
  display.setCursor(0, 44); display.println("RX->GPIO21");
  display.display();
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Energy Monitor Sender ===");

  // Flash filesystem
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
  }

  // Load saved config
  loadConfig();

  // Load saved energy base
  if (LittleFS.exists("/energy.txt")) {
    File file = LittleFS.open("/energy.txt", "r");
    baseEnergy = file.readString().toFloat();
    file.close();
    Serial.printf("Loaded base energy: %.4f kWh\n", baseEnergy);
  }

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("SSD1306 init failed! Check OLED wiring.");
    // Don't halt — continue without display
  } else {
    showSplash();
  }

  // Buttons
  pinMode(ENERGY_RESET_BTN, INPUT_PULLUP);
  pinMode(WIFI_CONFIG_BTN,  INPUT_PULLUP);

  // PZEM UART
  pzemSerial.begin(9600, SERIAL_8N1, 20, 21);
  delay(100);

  // WiFi + ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_max_tx_power(78);  // ~19.5 dBm

  initEspNow();

  Serial.println("Setup complete. Monitoring...");
}

// ==================== LOOP ====================
void loop() {

  // --- Config mode: just serve web requests ---
  if (configMode) {
    server.handleClient();
    return;
  }

  static unsigned long lastRead      = 0;
  static unsigned long lastEnergySave = 0;

  // ---- 1. WiFi Config Button (GPIO 2, hold 5s) ----
  if (digitalRead(WIFI_CONFIG_BTN) == LOW) {
    unsigned long pressStart = millis();
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20); display.println("Hold 5s for");
    display.setCursor(0, 32); display.println("Config Mode...");
    display.display();

    while (digitalRead(WIFI_CONFIG_BTN) == LOW) {
      if (millis() - pressStart >= 5000) {
        startConfigMode();
        return;
      }
      delay(50);
    }
    // Released before 5s — just refresh display on next read
  }

  // ---- 2. Energy Reset Button (GPIO 10, hold 3s) ----
  if (digitalRead(ENERGY_RESET_BTN) == LOW) {
    unsigned long pressStart = millis();
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20); display.println("Hold 3s to");
    display.setCursor(0, 32); display.println("Reset Energy...");
    display.display();

    while (digitalRead(ENERGY_RESET_BTN) == LOW) {
      if (millis() - pressStart >= 3000) {
        pzem.resetEnergy();
        baseEnergy = 0.0;
        myData.energy = 0.0;
        saveEnergy(0.0);

        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(10, 16); display.println("ENERGY");
        display.setCursor(10, 38); display.println("RESET!");
        display.display();
        delay(1500);
        Serial.println("Energy reset done.");
        break;
      }
      delay(50);
    }
  }

  // ---- 3. Read PZEM and send every 2 seconds ----
  if (millis() - lastRead >= 2000) {
    lastRead = millis();

    float v = pzem.voltage();

    if (isnan(v)) {
      Serial.println("PZEM read failed (NaN) — no AC load or wiring issue");
      showNoSensor();
    } else {
      myData.voltage = v;
      myData.power   = pzem.power();
      float rawEnergy = pzem.energy();
      if (isnan(rawEnergy)) rawEnergy = 0.0;
      myData.energy  = baseEnergy + rawEnergy;

      Serial.printf("V:%.1fV  P:%.0fW  E:%.4fkWh\n",
        myData.voltage, myData.power, myData.energy);

      // Send via ESP-NOW
      esp_err_t result = esp_now_send(broadcastAddress,
        (uint8_t *)&myData, sizeof(myData));
      if (result != ESP_OK) {
        Serial.println("esp_now_send error: " + String(esp_err_to_name(result)));
      }

      showReadings();
    }

    // Save energy to flash every 60 seconds to reduce flash wear
    if (millis() - lastEnergySave >= 60000) {
      lastEnergySave = millis();
      saveEnergy(myData.energy);
      Serial.printf("Energy saved to flash: %.4f kWh\n", myData.energy);
    }
  }
}
