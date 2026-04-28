#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PZEM004Tv30.h>

// Pins
#define OLED_MOSI  5
#define OLED_CLK   4
#define OLED_DC    3
#define OLED_CS    7
#define OLED_RST   -1 
#define ENERGY_RESET_BTN 10 // කරන්ට් බිල රිසෙට් කරන්න
#define WIFI_CONFIG_BTN  2  // Config Mode එකට යන්න (අලුත් බටන් එක)

Adafruit_SSD1306 display(128, 64, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RST, OLED_CS);
WebServer server(80);
HardwareSerial pzemSerial(1);
PZEM004Tv30 pzem(pzemSerial, 20, 21);

typedef struct struct_message {
    float voltage;
    float power;
    float energy;
} struct_message;

struct_message myData;
uint8_t broadcastAddress[6]; 
bool configMode = false;
float baseEnergy = 0.0;

// --- Load MAC from Flash ---
void loadConfig() {
  if (LittleFS.exists("/mac.txt")) {
    File f = LittleFS.open("/mac.txt", "r");
    String macStr = f.readString();
    f.close();
    sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
           &broadcastAddress[0], &broadcastAddress[1], &broadcastAddress[2], 
           &broadcastAddress[3], &broadcastAddress[4], &broadcastAddress[5]);
  } else {
    uint8_t defaultMac[] = {0x68, 0xFE, 0x71, 0xF7, 0xF7, 0x4C};
    memcpy(broadcastAddress, defaultMac, 6);
  }
}

// --- Web UI ---
void handleRoot() {
  String html = "<html><body style='font-family:sans-serif; text-align:center; padding-top:50px;'>";
  html += "<h1>MAC Configuration</h1>";
  html += "<form action='/save' method='POST'>";
  html += "Enter Receiver MAC: <input type='text' name='mac' placeholder='68:FE:71:F7:F7:4C' style='padding:10px;'><br><br>";
  html += "<input type='submit' value='Save & Restart' style='padding:10px 20px; background:#27ae60; color:white; border:none;'>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  String newMac = server.arg("mac");
  File f = LittleFS.open("/mac.txt", "w");
  f.print(newMac);
  f.close();
  server.send(200, "text/html", "Saved! Restarting...");
  delay(2000);
  ESP.restart();
}

void startConfigMode() {
  configMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ENERGY_MONITOR_CONFIG", "12345678");
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("--- CONFIG MODE ---");
  display.println("");
  display.println("Connect to WiFi:");
  display.println("ENERGY_MONITOR_CONFIG");
  display.println("");
  display.println("IP: 192.168.4.1");
  display.display();
}

void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin(true);
  loadConfig(); 

  display.begin(SSD1306_SWITCHCAPVCC);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();
  
  pinMode(ENERGY_RESET_BTN, INPUT_PULLUP);
  pinMode(WIFI_CONFIG_BTN, INPUT_PULLUP); // අලුත් බටන් එක

  // Normal ESP-NOW Setup
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() == ESP_OK) {
    esp_now_register_send_cb(onDataSent);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  if(LittleFS.exists("/energy.txt")) {
    File file = LittleFS.open("/energy.txt", FILE_READ);
    baseEnergy = file.readString().toFloat();
    file.close();
  }

  pzemSerial.begin(9600, SERIAL_8N1, 20, 21);
}

void loop() {
  if (configMode) {
    server.handleClient();
    return;
  }

  static unsigned long lastRead = 0;

  // 1. Config Button Logic (GPIO 2 - තත්පර 5ක් ඔබාගෙන ඉන්න)
  if (digitalRead(WIFI_CONFIG_BTN) == LOW) {
    unsigned long pressTime = millis();
    while(digitalRead(WIFI_CONFIG_BTN) == LOW) {
      if(millis() - pressTime > 5000) {
        startConfigMode();
        return;
      }
    }
  }

  // 2. Energy Reset Logic (GPIO 10 - තත්පර 3ක් ඔබාගෙන ඉන්න)
  if (digitalRead(ENERGY_RESET_BTN) == LOW) {
    delay(3000);
    if (digitalRead(ENERGY_RESET_BTN) == LOW) {
      baseEnergy = 0; pzem.resetEnergy();
      File f = LittleFS.open("/energy.txt", "w"); f.print("0.0"); f.close();
      display.clearDisplay(); display.setCursor(0,20); display.print("ENERGY RESET!"); display.display();
      delay(1000);
    }
  }

  // 3. Normal PZEM Task
  if (millis() - lastRead > 2000) {
    float v = pzem.voltage();
    if (!isnan(v)) {
      myData.voltage = v;
      myData.power = pzem.power();
      myData.energy = baseEnergy + pzem.energy();
      esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0,0);  display.printf("V:%.1f", myData.voltage);
      display.setCursor(0,22); display.printf("P:%.0f", myData.power);
      display.setCursor(0,44); display.printf("E:%.3f", myData.energy);
      display.display();
    }
    lastRead = millis();
  }
}
