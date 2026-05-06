# Industrial Energy Monitor — ESP-NOW & Web Config
### ESP32-C3 Super Mini + PZEM-004T + SSD1306 OLED (SPI)

A professional-grade wireless energy monitoring system that measures real-time Voltage, Power, and Energy consumption using a PZEM-004T v3.0 sensor. Data is transmitted wirelessly via ESP-NOW to a receiver and displayed live on a 128×64 SPI OLED screen. The receiver MAC address can be updated over-the-air using a built-in web interface — no re-flashing needed.

---

## 🚀 Key Features

| Feature | Details |
| :--- | :--- |
| **Accurate Monitoring** | Measures Voltage (V), Power (W), and Energy (kWh) via PZEM-004T v3.0 |
| **Wireless (ESP-NOW)** | Low-latency, connectionless 2.4GHz transmission every 2 seconds |
| **Real-time OLED Display** | Labeled V / W / kWh readings on 128×64 SPI SSD1306 |
| **Web Configuration** | Update receiver MAC via smartphone browser — no code changes needed |
| **Persistent Energy Storage** | Energy saved to LittleFS flash every 60s — survives power cycles |
| **Flash Wear Protection** | Energy written every 60s (not every 2s) to extend flash lifespan |
| **Hardware Buttons** | Energy Reset (GPIO 10) and WiFi Config (GPIO 2) |
| **Error Handling** | PZEM read failures shown on OLED; ESP-NOW errors printed to Serial |
| **ESP32 Core 3.x Compatible** | Uses `wifi_tx_info_t` send callback — works on Arduino core 3.3.8+ |

---

## 🔌 Complete Wiring (Sender — ESP32-C3 Super Mini)

### OLED SSD1306 SPI (7-pin)

| OLED Pin | Connect To | Notes |
| :--- | :--- | :--- |
| **GND** | GND rail | Common ground |
| **VCC** | ESP32 **3V3 pin** | ⚠ 3.3V ONLY — never 5V |
| **D0** | GPIO 4 | SPI Clock (CLK) |
| **D1** | GPIO 5 | SPI Data (MOSI) |
| **RES** | ESP32 **3V3 pin** | Tie HIGH — RST=-1 in code |
| **DC** | GPIO 3 | Data / Command select |
| **CS** | GPIO 7 | Chip Select |

> **RES must be tied to 3.3V**, not left floating. Floating RES causes random display lockups.

### PZEM-004T v3.0

| PZEM Pin | Connect To | Notes |
| :--- | :--- | :--- |
| **VCC** | 5V (USB or external) | 5V power |
| **GND** | GND rail | Shared common ground |
| **TX** | GPIO 20 | ESP32 UART RX |
| **RX** | GPIO 21 | ESP32 UART TX |
| **L / N** | AC Mains input | ⚠ High voltage — use caution |
| **L-out / N-out** | Load appliance | Switched AC output |

> PZEM-004T v3.0 accepts 3.3V logic signals on TX/RX despite running on 5V — no level shifter needed.

### Buttons

| Button | GPIO | Other Pin | Action |
| :--- | :--- | :--- | :--- |
| Energy Reset | GPIO 10 | GND | Hold 3 seconds |
| WiFi Config | GPIO 2 | GND | Hold 5 seconds |

### Power Rules

| Situation | USB to PC | External 5V to VIN |
| :--- | :--- | :--- |
| Development / uploading code | ✅ Connected | ❌ Do NOT connect |
| Final installed unit | ❌ Disconnected | ✅ Connect |
| Both at same time | ☠️ Never | ☠️ Never |

> When USB is connected, take PZEM 5V from the ESP32's **5V/VIN output pin** — it passes USB 5V through.  
> When running on external supply, connect external 5V → ESP32 VIN and external 5V → PZEM VCC with a **shared GND**.

---

## 📦 Required Libraries

Install all via **Arduino IDE → Library Manager**:

| Library | Author |
| :--- | :--- |
| `Adafruit SSD1306` | Adafruit |
| `Adafruit GFX Library` | Adafruit |
| `PZEM-004T-v30` | Olexi |
| `LittleFS` | Built-in (ESP32 core) |
| `ESP-NOW` | Built-in (ESP32 core) |

**Board Package:** `esp32` by Espressif — version **3.3.8 or newer**  
Board: `Nologo ESP32C3 Super Mini`

---

## ⚙️ Arduino IDE Board Settings (Sender)

| Setting | Value |
| :--- | :--- |
| Board | Nologo ESP32C3 Super Mini |
| USB Mode | Hardware CDC and JTAG |
| Flash Size | Default 4MB with SPIFFS |
| Partition Scheme | 1.2MB APP / 1.5MB SPIFFS |
| CPU Frequency | 160MHz (WiFi) |
| Flash Mode | QIO |
| Flash Frequency | 80MHz |
| Upload Speed | 921600 |

---

## 🛠 Operating Instructions

### 1. First-Time Setup

1. Flash `resivercode.ino` to the receiver ESP32
2. Open Serial Monitor at **115200 baud**
3. Note the MAC address printed:
   ```
   This receiver MAC: 68:FE:71:F7:F7:4C
   ```
4. Flash `sender_esp32c3code.ino` to the sender ESP32-C3
5. On the sender, hold **GPIO 2 button for 5 seconds**
6. Connect phone to WiFi: `ENERGY_MONITOR_CONFIG` (Password: `12345678`)
7. Open browser → `192.168.4.1`
8. Enter the receiver MAC → Save & Restart

### 2. Normal Monitoring Mode

- Starts automatically on every boot
- OLED shows **Voltage**, **Power**, and **Energy** with labels
- Data sent every **2 seconds** via ESP-NOW
- Energy accumulated and saved to flash every **60 seconds**

### 3. Configuration Mode (Update Receiver MAC)

- Hold **GPIO 2** button for **5 seconds**
- OLED shows config details
- Connect to `ENERGY_MONITOR_CONFIG` WiFi (pass: `12345678`)
- Go to `192.168.4.1` in browser
- Enter new receiver MAC → Save → device restarts with new MAC

### 4. Energy Reset

- Hold **GPIO 10** button for **3 seconds**
- Resets accumulated kWh in PZEM sensor and flash memory
- OLED shows "ENERGY RESET!" confirmation

---

## 📡 Stability & Optimizations

| Optimization | Detail |
| :--- | :--- |
| **WiFi Power Save** | Disabled (`WIFI_PS_NONE`) — no packet drops |
| **TX Power** | 19.5 dBm (level 78) — max stable range |
| **Fixed Channel** | Channel 1 (both sender and receiver locked) |
| **ESP-NOW deinit** | Called before switching to AP config mode |
| **Flash wear reduction** | Energy written every 60s, not every 2s |
| **NaN guard** | PZEM NaN readings caught and displayed as error |
| **MAC validation** | sscanf result checked — falls back to default on corrupt file |

---

## 🔍 Serial Monitor Output

**Sender (115200 baud):**
```
=== Energy Monitor Sender ===
Loaded MAC: 68:FE:71:F7:F7:4C
Loaded base energy: 0.0125 kWh
Setup complete. Monitoring...
V:229.8V  P:45W  E:0.0125kWh
[ESP-NOW] Sent OK
```

**Receiver (115200 baud):**
```
=== Energy Monitor Receiver ===
This receiver MAC: 68:FE:71:F7:F7:4C
>>> Enter this MAC in the sender's web config page <<<
Ready. Waiting for data on channel 1...

==============================
  From     : A0:B7:65:12:34:56
  Packet # : 1
  VOLTAGE  : 229.8 V
  POWER    : 45.0 W
  ENERGY   : 0.0125 kWh
==============================
```

---

## 📁 File Structure

```
project/
├── sender_esp32c3code.ino   — Sender: PZEM read, OLED display, ESP-NOW TX, Web config
├── resivercode.ino          — Receiver: ESP-NOW RX, Serial output
└── README.md                — This file
```

---

*Developed for Industrial IoT Monitoring Applications.*  
*Compatible with ESP32 Arduino Core 3.3.8+*
