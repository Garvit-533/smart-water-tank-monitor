<div align="center">
  
  # 🚰 Smart Water Tank Monitoring & Automation System
  
  [![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)]()
  [![ESP8266](https://img.shields.io/badge/ESP8266-000000?style=for-the-badge&logo=espressif&logoColor=white)]()
  [![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)]()
  [![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=for-the-badge&logo=css3&logoColor=white)]()
  [![IoT](https://img.shields.io/badge/IoT-FF9900?style=for-the-badge&logo=awsiot&logoColor=white)]()

  > An embedded C++ IoT system that transforms an ESP8266 into an autonomous water management controller featuring real-time ultrasonic depth sensing, TFT animations, and a monolithic web dashboard.
  
</div>

---

## 🌟 Features & Engineering Highlights

*   **Monolithic Web Server:** The entire mobile-responsive frontend is minified and served directly from a C++ string within the firmware, eliminating the need for external flash file systems.
*   **Asynchronous Architecture:** Replaced blocking delays with a custom `millis()` state machine, allowing smooth ST7735 TFT wave animations to render without stalling HTTP requests[cite: 1].
*   **Autonomous Safety Logic:** Continuously evaluates water levels to enforce an automatic active-low relay cutoff at 100% capacity, preventing tank overflow.
*   **Signal Processing:** Implemented a 5-point moving average algorithm to filter noisy ultrasonic sensor readings and prevent false trigger events[cite: 1].
*   **Hardware Debouncing:** Engineered stable physical button states utilizing time-based edge detection to eliminate relay chattering during manual overrides[cite: 1].

---

## 🧰 Tech Stack & Hardware

| Component | Function | NodeMCU Pin | ESP8266 GPIO |
| :--- | :--- | :--- | :--- |
| **ST7735 TFT** | CS, RST, DC | D8, D4, D3 | GPIO 15, 2, 0[cite: 1] |
| **HC-SR04** | Ultrasonic Trigger & Echo | TX, D6 | GPIO 1, 12[cite: 1] |
| **5V Relay** | Pump Control (Active-LOW) | D1 | GPIO 5[cite: 1] |
| **Push Button**| Manual Toggle (Pull-up) | D2 | GPIO 4[cite: 1] |
| **Flow Sensor**| Droplet State (Pull-up) | RX | GPIO 3[cite: 1] |

---

## ⚙️ Installation & Setup

**1. Clone & Configure**
```bash
git clone [https://github.com/Garvit-533/smart-water-tank-monitor.git](https://github.com/Garvit-533/smart-water-tank-monitor.git)
```
Open `WATER_TANK_MCU.ino` in the Arduino IDE[cite: 1]. Update the network variables at the top of the file with your local Wi-Fi details:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

**2. Compile & Upload**
Select **NodeMCU 1.0 (ESP-12E Module)** from the boards menu. Ensure required libraries (`Adafruit_GFX`, `Adafruit_ST7735`, `ESP8266WiFi`, `ESP8266WebServer`) are installed, then compile and upload via USB[cite: 1].

**3. Access Dashboard**
Navigate to the configured static IP (`http://192.168.1.102`) in any web browser connected to the local network[cite: 1].

---

## 📄 License
This project is open-source and available under the [MIT License](LICENSE).
