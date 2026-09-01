<div align="center">
  
  # 🚰 Smart Water Tank Monitoring & Automation System
  
  [![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)]()
  [![ESP8266](https://img.shields.io/badge/ESP8266-000000?style=for-the-badge&logo=espressif&logoColor=white)]()
  [![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)]()
  [![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=for-the-badge&logo=css3&logoColor=white)]()
  [![IoT](https://img.shields.io/badge/IoT-FF9900?style=for-the-badge&logo=awsiot&logoColor=white)]()

  > An advanced embedded C++ IoT system that transforms an ESP8266 into an autonomous water management controller. Features real-time ultrasonic depth sensing, flicker-free TFT animations, and an embedded asynchronous web dashboard.
  
</div>

---

## 🌟 System Architecture & Engineering Highlights

### 1. Monolithic Firmware & Asynchronous Web Server
*   **Zero External File Systems:** The entire mobile-responsive frontend (HTML, CSS animations, JavaScript) is minified and served directly from a C++ string within the firmware memory, simplifying OTA deployments.
*   **REST-like API JSON Polling:** The web client utilizes asynchronous `fetch()` API calls to a `/data` endpoint every 1000ms, updating the DOM dynamically without requiring page refreshes.
*   **Non-Blocking State Machine:** Replaced standard blocking `delay()` functions with a custom `millis()` polling architecture, ensuring HTTP requests, sensor reads, and UI animations run concurrently without thread starvation.

### 2. Advanced Signal Processing & Control Logic
*   **Moving-Average Filtering:** Implemented a 5-point moving average algorithm to sanitize noisy ultrasonic sensor readings, rejecting out-of-bound anomalies (e.g., timeouts or reflections).
*   **Autonomous Safety Cutoff:** The firmware continuously maps distance (20cm to 100cm) to a 0-100% capacity range. If capacity reaches 100%, the active-low relay is automatically disengaged to prevent overflow and dry-running, even if manually overridden.
*   **Hardware Debouncing:** Engineered a 40ms time-based edge detection algorithm for the physical push-button, completely eliminating relay chattering during manual overrides.

### 3. Optimized TFT Rendering Engine
*   **Flicker-Free Updates:** Instead of clearing the entire screen (which causes severe flickering on ST7735 displays), the code surgically redraws only the specific bounding boxes of changing pixels (like the water level and percentage text).
*   **Trigonometric Wave Animations:** Utilizes sine wave math (`sin()`) to dynamically generate and animate a fluid water surface inside the on-screen tank.
*   **Interrupt-Style Flow Monitoring:** The RX pin is monitored constantly in the main loop; grounding it instantly flags a state change to render a blinking droplet icon, providing immediate visual feedback for active water flow.

---

## 🔌 API Endpoints Reference

| Endpoint | Method | Description | Response Type |
| :--- | :--- | :--- | :--- |
| `/` | `GET` | Serves the main HTML/CSS/JS web dashboard. | `text/html` |
| `/data` | `GET` | Returns real-time telemetry (level, distance, relay state). | `application/json` |
| `/toggle` | `POST` | Toggles the water pump relay state remotely. | `text/plain` (200 OK) |

---

## 🧰 Hardware & Pin Mapping

| Component | Function | NodeMCU Pin | ESP8266 GPIO |
| :--- | :--- | :--- | :--- |
| **ST7735 TFT (1.8")** | CS, RST, DC | D8, D4, D3 | GPIO 15, 2, 0 |
| **HC-SR04 Sensor** | Ultrasonic Trigger & Echo | TX, D6 | GPIO 1, 12 |
| **5V Relay Module** | Pump Control (Active-LOW) | D1 | GPIO 5 |
| **Push Button**| Manual Toggle (Pull-up) | D2 | GPIO 4 |
| **Flow Sensor/Wire**| Droplet State (Pull-up) | RX | GPIO 3 |

---

## ⚙️ Installation & Setup

**1. Clone & Configure**
```bash
git clone [https://github.com/Garvit-533/smart-water-tank-monitor.git](https://github.com/Garvit-533/smart-water-tank-monitor.git)
```
Open `WATER_TANK_MCU.ino` in the Arduino IDE. Secure the project by updating the network variables at the top of the file with your local Wi-Fi details:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

**2. Network Configuration**
The system is configured with a static IP for reliable local access. Modify these variables if your router uses a different subnet:
```cpp
IPAddress staticIP(192, 168, 1, 102);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
```

**3. Compile & Upload**
Select **NodeMCU 1.0 (ESP-12E Module)** from the boards menu. Ensure required libraries (`Adafruit_GFX`, `Adafruit_ST7735`, `ESP8266WiFi`, `ESP8266WebServer`) are installed via the Library Manager. Compile and upload via USB.

**4. Access Dashboard**
Navigate to the configured static IP (`http://192.168.1.102`) in any web browser connected to the local network to view the interface and control the pump.

---

## 📄 License
This project is open-source and available under the [MIT License](LICENSE).
