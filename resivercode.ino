#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

typedef struct struct_message {
    float voltage;
    float power;
    float energy;
} struct_message;

struct_message incomingData;

void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  
  Serial.println("\n--- DATA RECEIVED ---");
  Serial.printf("VOLTAGE : %.1f V\n", incomingData.voltage);
  Serial.printf("POWER   : %.0f W\n", incomingData.power);
  Serial.printf("ENERGY  : %.3f kWh\n", incomingData.energy);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  WiFi.mode(WIFI_STA);
  
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
  Serial.println("Ready! Channel 1. Waiting...");
}

void loop() {}
