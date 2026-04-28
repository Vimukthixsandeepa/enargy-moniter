# Industrial Energy Monitor with ESP-NOW & Web Config

This project is a professional-grade industrial energy monitoring system built using an ESP32-C3 Super Mini, a PZEM-004T energy sensor, and an OLED display. It features stable wireless data transmission via ESP-NOW and a web-based configuration interface for dynamic MAC address updates.

## 🚀 Key Features
- **Accurate Monitoring:** Measures Voltage, Power, and Energy using the PZEM-004T v3.0 sensor.
- **Stable Wireless:** Utilizes ESP-NOW with optimized WiFi power settings for reliable data delivery.
- **Real-time Display:** Data visualization on an SSD1306 SPI OLED display.
- **Web Configuration:** Dedicated mode (GPIO 2) to update the receiver's MAC address via a smartphone without re-coding.
- **Persistent Storage:** Energy readings and MAC addresses are stored in LittleFS to survive power cycles.
- **Hardware Controls:** Dedicated buttons for Energy Reset (GPIO 10) and WiFi Config (GPIO 2).

## 🔌 Pin Mapping (ESP32-C3 Super Mini)
| Component | Pin | Function |
| :--- | :--- | :--- |
| **OLED MOSI** | GPIO 5 | SPI Data |
| **OLED CLK** | GPIO 4 | SPI Clock |
| **OLED DC** | GPIO 3 | Data/Command |
| **OLED CS** | GPIO 7 | Chip Select |
| **PZEM TX** | GPIO 21 | UART RX on ESP32 |
| **PZEM RX** | GPIO 20 | UART TX on ESP32 |
| **Energy Reset**| GPIO 10 | GND to Reset (Hold 3s) |
| **WiFi Config** | GPIO 2 | GND to Config (Hold 5s) |

## 🛠 Operating Instructions

### 1. Normal Monitoring Mode
- The system starts in this mode by default.
- OLED displays: **Voltage (V)**, **Power (P)**, and **Energy (E)**.
- Data is transmitted every 2 seconds via ESP-NOW to the saved MAC address.

### 2. Configuration Mode (Dynamic MAC Update)
- **Action:** Hold the button on **GPIO 2** for 5 seconds while the system is running.
- **Process:**
  1. OLED will display "CONFIG MODE ON".
  2. Connect your phone to WiFi: `ENERGY_MONITOR_CONFIG` (Password: `12345678`).
  3. Open a browser and go to: `192.168.4.1`.
  4. Enter the new Receiver MAC (e.g., `68:FE:71:F7:F7:4C`) and save.
  5. The device will restart and use the new MAC.

### 3. Energy Reset
- **Action:** Hold the button on **GPIO 10** for 3 seconds.
- **Process:** Resets the accumulated kWh both in the PZEM sensor and the Flash memory.

## 📡 Stability & Optimization
- **WiFi Stability:** Power save mode is disabled (`WIFI_PS_NONE`) to ensure no data packets are dropped.
- **Transmit Power:** Adjusted to 19dBm (Level 78) to balance range and power consumption.
- **Storage:** Uses `LittleFS` for robust data handling on the ESP32 internal flash.

---
*Developed for Industrial IoT Monitoring Applications.*
