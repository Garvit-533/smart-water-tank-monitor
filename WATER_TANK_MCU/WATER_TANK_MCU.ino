#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <math.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";

// Static IP configuration
IPAddress staticIP(192, 168, 1, 102);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Web server on port 80
ESP8266WebServer server(80);

/*
  WATER TANK display sketch — updated so droplet depends ONLY on RX pin.
  - Preserves all features: ultrasonic averaging, relay active-low, auto-off at 100%, button debounce, wave animation.
  - Fix: whenever RX pin state changes, the display is flagged to update so the droplet appears/disappears immediately.
*/

// Pin definitions (kept same)
#define RX_CHECK_PIN 3  // GPIO 3 = RX (reads LOW when grounded)

#define TFT_CS     15  // GPIO 15 (D8)
#define TFT_RST    2   // GPIO 2 (D4)
#define TFT_DC     0   // GPIO 0 (D3)

#define BUTTON_PIN 4   // GPIO 4 (D2)
#define RELAY_PIN  5   // GPIO 5 (D1)

#define TRIG_PIN 1     // GPIO1 (TX) - original layout preserved
#define ECHO_PIN 12

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ---------- Configuration ----------
const unsigned long SENSOR_INTERVAL = 1000UL; // ms between ultrasonic reads
const int NUM_READINGS = 5;                   // moving average window
const int MAX_VALID_DISTANCE = 400;           // cm (safety)
const int TANK_MAX_CM = 100;                  // assume 100cm tank height for mapping

// Wave params
float wavePhase = 0.0f;
#define WAVE_SPEED 0.06f
#define WAVE_AMPL 6
#define WAVE_LEN 36

// Colors
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
const uint16_t COL_BLACK = ST77XX_BLACK;
const uint16_t COL_WHITE = ST77XX_WHITE;
const uint16_t COL_BLUE = rgb(0, 40, 180);
const uint16_t COL_CYAN = rgb(60, 180, 200);
const uint16_t COL_LIGHTMETAL = rgb(200,200,205);
const uint16_t COL_DARKMETAL = rgb(70,70,80);
const uint16_t COL_GLASS_EDGE = rgb(150, 200, 230);
const uint16_t COL_WATER_SURF = rgb(180,220,255);
const uint16_t COL_ORANGE = rgb(255,160,30);
const uint16_t COL_RED = ST77XX_RED;
const uint16_t COL_YELLOW = ST77XX_YELLOW;

// Tank layout (kept same)
const int cylX = 4, cylY = 53, cylW = 69, cylH = 110;
const int glassInset = 8;
const int glassX = cylX + glassInset;
const int glassY = cylY + 14;
const int glassW = cylW - 2 * glassInset;
const int glassH = cylH - 30;
const int pctX = 20, pctY = 26;

// ---------- State ----------
bool relayState = false;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_MS = 40;

int readings[NUM_READINGS];
int readIndex = 0;
long total = 0;
int averageDistance = -1;
int prevDistance = -1;

int displayedPct = -1;      // last drawn percentage
bool displayedRelay = false;
bool displayedWaterAvail = false;

unsigned long lastSensorRead = 0;
bool displayNeedsUpdate = true;

// textual states (kept as Strings for display parity)
String waterLevel = "0%";
String waterAvail = "NO";
String prevWaterLevel = "";
String prevWaterAvail = "";

// RX tracking (FIX)
int prevRxState = HIGH; // initial assumed HIGH due to INPUT_PULLUP

// Forward declarations
void drawStaticTank();
void drawTopRim(int cx, int cy, int w, int h, uint16_t color);
void drawBottomRim(int cx, int cy, int w, int h, uint16_t color);
void drawWater(int pct, float phase);
void drawWaveSurface(int lvlY, float phase, int glassLeft, int glassTop, int glassWidth, int glassHeight);
void drawDroplet(int cx, int cy, float s);
void drawMotor(int cx, int cy, float s);
void drawPercentBig(int pct);

void setup() {
  Serial.begin(9600);

  // TFT init
  tft.initR(INITR_BLACKTAB);     // 1.8" ST7735
  tft.setSPISpeed(40000000);     // attempt high-speed SPI
  tft.setRotation(0);
  tft.fillScreen(COL_BLACK);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // relay off initially (active-low)

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);

  // RX pin: use internal pull-up so grounding it gives LOW
  pinMode(RX_CHECK_PIN, INPUT_PULLUP);
  prevRxState = digitalRead(RX_CHECK_PIN);

  // init moving average
  total = 0;
  averageDistance = 0;
  for (int i = 0; i < NUM_READINGS; ++i) {
    readings[i] = 0;
  }

  drawStaticTank();
  // initial draw
  displayNeedsUpdate = true;

  // Connect to WiFi with static IP
  WiFi.config(staticIP, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Start web server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.begin();
  Serial.println("Web server started");
}

// Web server handlers
void handleRoot() {
  String html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Water Tank Monitor</title><style>*{margin:0;padding:0;box-sizing:border-box;font-family:Poppins,sans-serif;}body{height:100vh;width:100vw;background:linear-gradient(180deg,#0b0f1a,#141a2e);color:white;display:flex;justify-content:center;align-items:center;}.tank-box{width:100%;max-width:440px;padding:20px;height:100vh;text-align:center;display:flex;flex-direction:column;justify-content:flex-start;align-items:center;}h2{font-size:28px;color:#71c9ff;text-shadow:0 0 10px #00b7ff;margin-top:25px;margin-bottom:25px;font-weight:600;}.circle{width:85vw;height:85vw;max-width:330px;max-height:330px;margin-bottom:25px;border-radius:50%;position:relative;overflow:hidden;background:rgba(0,0,0,0.45);box-shadow:inset 0 0 30px rgba(0,200,255,0.4),0 0 25px rgba(0,200,255,0.2);}.wave{position:absolute;bottom:0;width:200%;height:200%;background:rgba(0,150,255,0.55);border-radius:45%;animation:waveMove 4s infinite linear;transform:translateX(-25%);}.wave2{opacity:0.65;animation-duration:7s;}@keyframes waveMove{from{transform:translateX(-25%) rotate(0deg);}to{transform:translateX(-25%) rotate(360deg);}}.percent{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);font-size:10vw;font-weight:700;text-shadow:0 0 10px #00b7ff;}.droplet{font-size:55px;margin-top:-5px;margin-bottom:30px;animation:dropBlink 1.2s infinite;color:#40d7ff;}@keyframes dropBlink{0%{opacity:1;transform:translateY(0);}50%{opacity:0.6;transform:translateY(6px);}100%{opacity:1;transform:translateY(0);}}.motor-btn{width:100%;padding:15px;font-size:18px;font-weight:bold;border:none;border-radius:12px;background:#00b7ff;color:white;box-shadow:0 0 15px #00b7ff;transition:0.3s;}.motor-btn.active{background:#ff4a4a;box-shadow:0 0 15px #ff4a4a;}.motor-btn:active{transform:scale(0.97);}</style></head><body><div class=\"tank-box\"><h2>WATER TANK</h2><div class=\"circle\"><div class=\"wave\" id=\"wave1\" style=\"top:40%;\"></div><div class=\"wave wave2\" id=\"wave2\" style=\"top:40%;\"></div><div class=\"percent\" id=\"pct\">75%</div></div><div class=\"droplet\" id=\"drop\" style=\"display:none;\">💧</div><button class=\"motor-btn\" id=\"motorBtn\">OFF</button></div><script>let motorOn=false;document.getElementById('motorBtn').onclick=function(){fetch('/toggle',{method:'POST'}).then(()=>{motorOn=!motorOn;this.classList.toggle('active');this.innerText=motorOn?'ON':'OFF';});};function update(){fetch('/data').then(r=>r.json()).then(d=>{document.getElementById('pct').innerText=d.level+'%';let pos=100-d.level;document.getElementById('wave1').style.top=pos+'%';document.getElementById('wave2').style.top=pos+'%';document.getElementById('drop').style.display=d.rxLow?'block':'none';motorOn=d.relay;let btn=document.getElementById('motorBtn');btn.classList.toggle('active',motorOn);btn.innerText=motorOn?'ON':'OFF';});}setInterval(update,1000);update();</script></body></html>";
  server.send(200, "text/html", html);
} 

void handleData() {
  String json = "{";
  json += "\"level\":" + String(waterLevel.toInt()) + ",";
  json += "\"distance\":" + String(averageDistance) + ",";
  json += "\"relay\":" + String(relayState ? "true" : "false") + ",";
  json += "\"available\":\"" + waterAvail + "\",";
  json += "\"rxLow\":" + String(digitalRead(RX_CHECK_PIN) == LOW ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleToggle() {
  int pct = waterLevel.toInt();
  if (pct == 100) {
    if (relayState) {
      relayState = false;
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Relay turned OFF: Tank full (web)");
    }
  } else {
    relayState = !relayState;
    digitalWrite(RELAY_PIN, relayState ? LOW : HIGH);
    Serial.print("Relay toggled to: ");
    Serial.println(relayState ? "ON" : "OFF");
  }
  displayNeedsUpdate = true;
  server.send(200, "text/plain", "OK");
}

void loop() {
  unsigned long now = millis();

  // ------------- Sensor reading (non-blocking frequency) -------------
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    // Trigger pulse (standard HC-SR04 style)
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL); // 30ms timeout (~5m)
    int currentDistance = 0;
    if (duration == 0) {
      // timeout: keep previous reading
      currentDistance = averageDistance;
    } else {
      // speed of sound approx 0.034 cm/us
      currentDistance = (int)round((duration * 0.034) / 2.0);
    }

    // moving average update
    total -= readings[readIndex];
    readings[readIndex] = currentDistance;
    total += readings[readIndex];
    readIndex = (readIndex + 1) % NUM_READINGS;

    int avg = (int)(total / NUM_READINGS);

    // filter invalid extremes
    if (avg < 2 || avg > MAX_VALID_DISTANCE) {
      // fallback to previous valid
      avg = (averageDistance >= 0) ? averageDistance : avg;
    }

    averageDistance = avg;

    // Map to percentage (assume sensor at top of tank, tank height TANK_MAX_CM)
    int percentage = 0;
    if (averageDistance <= 20) {
    percentage = 100;
}
else if (averageDistance >= 100) {
    percentage = 0;
}
else {
    percentage = map(averageDistance, 20, 100, 100, 0);
}

    // clamp
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    // update textual states
    waterLevel = String(percentage) + "%";
    waterAvail = (percentage > 0) ? "YES" : "NO";

    // Serial log
    Serial.print("Distance: ");
    Serial.print(averageDistance);
    Serial.print(" cm, Percentage: ");
    Serial.print(percentage);
    Serial.println("%");

    // trigger any auto-relay off if full
    if (percentage >= 100 && relayState) {
      relayState = false;
      digitalWrite(RELAY_PIN, HIGH); // off (active-low)
      Serial.println("Auto Relay OFF: Tank reached 100%");
    }

    // mark display update if changed
    if (averageDistance != prevDistance) {
      displayNeedsUpdate = true;
      prevDistance = averageDistance;
    }
    if (waterLevel != prevWaterLevel) {
      displayNeedsUpdate = true;
      prevWaterLevel = waterLevel;
    }
    if (waterAvail != prevWaterAvail) {
      displayNeedsUpdate = true;
      prevWaterAvail = waterAvail;
    }
  }

  // ------------- RX pin monitoring (important fix) -------------
  // Always check RX pin each loop; if it changed, request redraw so droplet updates immediately
  int rxState = digitalRead(RX_CHECK_PIN); // HIGH if open, LOW if grounded
  if (rxState != prevRxState) {
    prevRxState = rxState;
    displayNeedsUpdate = true; // force redraw so droplet appears/disappears immediately
    // Optional: debug
    Serial.print("RX state changed -> ");
    Serial.println(rxState == LOW ? "GROUND(LOW)" : "OPEN(HIGH)");
  }

  // ------------- Button handling (debounced) -------------
  bool rawButton = digitalRead(BUTTON_PIN);
  // simple edge detect with debounce
  if (rawButton != lastButtonState) {
    lastDebounceTime = now;
  }
  if ((now - lastDebounceTime) > DEBOUNCE_MS) {
    // stable state
    static bool stableState = HIGH;
    if (rawButton != stableState) {
      stableState = rawButton;
      if (stableState == LOW) { // pressed (active LOW)
        int pct = waterLevel.toInt();
        if (pct == 100) {
          // If full, ensure motor off
          if (relayState) {
            relayState = false;
            digitalWrite(RELAY_PIN, HIGH);
            Serial.println("Relay turned OFF: Tank full (button)");
          }
        } else {
          // toggle relay
          relayState = !relayState;
          digitalWrite(RELAY_PIN, relayState ? LOW : HIGH);
          Serial.print("Relay toggled to: ");
          Serial.println(relayState ? "ON" : "OFF");
        }
        displayNeedsUpdate = true;
      }
    }
  }
  lastButtonState = rawButton;

  // ------------- Display update (only when needed) -------------
  if (displayNeedsUpdate) {
    // Advance wave
    wavePhase += WAVE_SPEED;
    if (wavePhase > 1e6) wavePhase = 0;

    int pct = waterLevel.toInt();
    drawWater(pct, wavePhase);

    // Droplet (top-right) area
    int dropCx = cylX + cylW + 29;
    int dropCy = cylY + 28;
    float dropS = 1.6f;
    // Clear droplet area every update (small rect)
    int dropH = (int)(18 * dropS);
    int dropW = (int)(12 * dropS);
    int dropR = dropW / 2;
    tft.fillRect(dropCx - dropR - 3, dropCy - dropH/2 - 3, dropW + 6, dropH + 8, COL_BLACK);

    // RX dependent droplet (ONLY RX, independent of percentage)
    bool rxDropEnabled = (digitalRead(RX_CHECK_PIN) == LOW);
    if (rxDropEnabled) {
      drawDroplet(dropCx, dropCy, dropS);
    }

    // Motor area - clear & redraw if on
    int motorCx = cylX + cylW + 30;
    int motorCy = cylY + cylH - 25;
    float motorS = 1.4f;
    tft.fillRect(motorCx - 22, motorCy - 18, 44, 36, COL_BLACK);
    if (relayState) {
      drawMotor(motorCx, motorCy, motorS);
    }

    // Big percentage (clears small area internally)
    drawPercentBig(pct);

    // Distance text - clear area then print
    // tft.fillRect(10, 148, 120, 14, COL_BLACK);
    // tft.setCursor(10, 150);
    // tft.setTextColor(COL_WHITE);
    // tft.setTextSize(1);
    // tft.print("Distance: ");
    // tft.print(averageDistance);
    // tft.print(" cm");

    // Update tracking flags & clear update flag
    displayedPct = pct;
    displayedRelay = relayState;
    displayedWaterAvail = (waterAvail == "YES");
    displayNeedsUpdate = false;
  }

  // Handle web server requests
  server.handleClient();

  // Small yield to keep CPU available (no long blocking)
  delay(10);
}

// ----------------- Draw static shell and glass -----------------
void drawStaticTank() {
  tft.fillScreen(COL_BLACK);

  // Title
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  tft.setCursor(7, 3);
  tft.print("WATER TANK");

  // Draw metal cylinder body with vertical gradient strips
  for (int i = 0; i < cylW; i++) {
    float rel = (float)i / (float)(cylW - 1);
    uint8_t b = (uint8_t)constrain(120 + rel * 110, 0, 255);
    uint8_t g = (uint8_t)constrain(120 + rel * 100, 0, 255);
    uint8_t r = (uint8_t)constrain(130 + rel * 90, 0, 255);
    uint16_t col = rgb(r, g, b);
    tft.fillRect(cylX + i, cylY + 12, 1, cylH - 24, col);
  }

  // Top and bottom rims
  int topCx = cylX + cylW/2;
  int topCy = cylY + 10;
  drawTopRim(topCx, topCy, cylW, 10, COL_DARKMETAL);

  int botCx = topCx;
  int botCy = cylY + cylH - 10;
  drawBottomRim(botCx, botCy, cylW, 10, COL_DARKMETAL);

  // Inner glass area: rounded rectangle (clear plus border)
  tft.fillRoundRect(glassX, glassY, glassW, glassH, 6, COL_BLACK); // clear inner
  tft.drawRoundRect(glassX, glassY, glassW, glassH, 6, COL_GLASS_EDGE);

  // subtle left highlight on glass
  for (int i = 0; i < 6; i++) {
    uint16_t col = rgb(180 - i*8, 210 - i*10, 235);
    tft.drawFastVLine(glassX + 3 + i, glassY + 6, glassH - 12, col);
  }
}

void drawTopRim(int cx, int cy, int w, int h, uint16_t color) {
  int half = w/2;
  for (int dy = -h/2; dy <= h/2; dy++) {
    float rel = (float)(dy) / (float)(h/2);
    float span = half * sqrt(max(0.0f, 1.0f - rel*rel));
    int left = cx - (int)span;
    int right = cx + (int)span;
    tft.drawFastHLine(left, cy + dy, right - left, color);
  }
}
void drawBottomRim(int cx, int cy, int w, int h, uint16_t color) {
  int half = w/2;
  for (int dy = -h/2; dy <= h/2; dy++) {
    float rel = (float)(dy) / (float)(h/2);
    float span = half * sqrt(max(0.0f, 1.0f - rel*rel));
    int left = cx - (int)span;
    int right = cx + (int)span;
    tft.drawFastHLine(left, cy + dy, right - left, color);
  }
}

// ----------------- Draw water (inner glass area) -----------------
void drawWater(int pct, float phase) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  int innerTop = glassY + 4;
  int innerBottom = glassY + glassH - 4;
  int innerH = innerBottom - innerTop;
  int fillPixels = map(pct, 0, 100, 0, innerH);
  int fillTopY = innerBottom - fillPixels; // baseline before wave

  // Clear only inner glass to avoid full-screen flicker
  tft.fillRoundRect(glassX + 1, glassY + 1, glassW - 2, glassH - 2, 5, COL_BLACK);

  // Fill water body bottom-up with alternating scanlines for texture
  for (int y = fillTopY; y <= innerBottom; y++) {
    uint16_t col = ((y % 6) < 3) ? rgb(0,40,120) : rgb(0,70,160);
    tft.drawFastHLine(glassX + 2, y, glassW - 4, col);
  }

  // subtle reflection near top
  if (fillPixels > 6) {
    int reflectY = constrain(fillTopY + 3, innerTop, innerBottom);
    tft.drawFastHLine(glassX + 10, reflectY, glassW - 20, rgb(160,220,255));
  }

  // Wave should show ONLY if RX droplet is active
bool rxActive = (digitalRead(RX_CHECK_PIN) == LOW);

if (fillPixels > 0 && rxActive) {
    drawWaveSurface(fillTopY, phase, glassX + 1, glassY + 1, glassW - 2, glassH - 2);
}


  // redraw glass border to keep crisp edges
  tft.drawRoundRect(glassX, glassY, glassW, glassH, 6, COL_GLASS_EDGE);
}

// draw soft sinusoidal wave across the glass area
void drawWaveSurface(int lvlY, float phase, int glassLeft, int glassTop, int glassWidth, int glassHeight) {
  for (int x = 0; x < glassWidth; x++) {
    float px = (float)x;
    float angle = (2.0f * PI * px / WAVE_LEN) + phase;
    float yoffset = sin(angle) * (WAVE_AMPL * 0.6f);
    int surfaceY = lvlY + (int)yoffset;
    surfaceY = constrain(surfaceY, glassTop, glassTop + glassHeight - 1);

    int drawX = glassLeft + x;
    // crest highlight
    tft.drawPixel(drawX, surfaceY, COL_WATER_SURF);
    // small blend below crest
    for (int k = 1; k <= 3; k++) {
      int yy = surfaceY + k;
      if (yy <= glassTop + glassHeight - 1) {
        uint16_t col = (k == 1) ? rgb(10,80,170) : rgb(0,50,140);
        tft.drawPixel(drawX, yy, col);
      }
    }
  }

  // foam dots spaced along crest
  for (int x = 0; x < glassWidth; x += 10) {
    float px = (float)x;
    float angle = (2.0f * PI * px / WAVE_LEN) + phase;
    float yoffset = sin(angle) * (WAVE_AMPL * 0.6f);
    int surfaceY = lvlY + (int)yoffset;
    int fx = glassLeft + x;
    int fy = surfaceY;
    if (fy - 1 >= glassTop) tft.drawPixel(fx, fy - 1, COL_WHITE);
  }
}

// ----------------- Droplet (top-right) -----------------
void drawDroplet(int cx, int cy, float s) {
  int h = (int)(18 * s);
  int w = (int)(12 * s);
  int r = w / 2;
  int bottomY = cy + h/4;

  // area cleared already by caller
  tft.fillCircle(cx, bottomY, r, COL_CYAN);
  tft.fillTriangle(cx, cy - h/2, cx - r, bottomY, cx + r, bottomY, COL_CYAN);
  tft.fillCircle(cx - r/3, bottomY - r/3, r/3, COL_WHITE);
}

// ----------------- Water Pump Icon -----------------
void drawMotor(int cx, int cy, float s) {
  int bodyR = (int)(10 * s);
  int bx = cx;
  int by = cy;

  // Pump round body
  tft.fillCircle(bx, by, bodyR, COL_DARKMETAL);
  tft.drawCircle(bx, by, bodyR, COL_WHITE);

  // Center shaft
  tft.fillCircle(bx, by, (int)(4 * s), COL_ORANGE);
  tft.drawCircle(bx, by, (int)(4 * s), COL_WHITE);

  // Inlet (left pipe)
  int pipeW = (int)(6 * s);
  int pipeH = (int)(6 * s);
  tft.fillRect(bx - bodyR - pipeW, by - pipeH/2, pipeW, pipeH, COL_LIGHTMETAL);
  tft.drawRect(bx - bodyR - pipeW, by - pipeH/2, pipeW, pipeH, COL_WHITE);

  // Outlet (right pipe)
  tft.fillRect(bx + bodyR, by - pipeH/2, pipeW, pipeH, COL_LIGHTMETAL);
  tft.drawRect(bx + bodyR, by - pipeH/2, pipeW, pipeH, COL_WHITE);

  // Flow arrow on outlet
  tft.drawLine(bx + bodyR + 2, by, bx + bodyR + pipeW - 2, by, COL_YELLOW);
  tft.drawTriangle(
      bx + bodyR + pipeW - 2, by,
      bx + bodyR + pipeW - 6, by - 3,
      bx + bodyR + pipeW - 6, by + 3,
      COL_YELLOW
  );

  // Base stand
  tft.fillRect(bx - (int)(10 * s), by + bodyR - 2, (int)(20 * s), (int)(4 * s), COL_DARKMETAL);
  tft.drawRect(bx - (int)(10 * s), by + bodyR - 2, (int)(20 * s), (int)(4 * s), COL_WHITE);
}

// ----------------- Draw big percentage (small area cleared only) -----------------
void drawPercentBig(int pct) {
  // area to clear - enough for "100%"
  tft.fillRect(pctX - 9, pctY - 1, 80, 30, COL_BLACK);

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(3);
  if (pct == 100) {
    tft.setCursor(pctX - 12, pctY);  // left shift for 100
  } else {
    tft.setCursor(pctX, pctY);
  }
  tft.print(pct);
  tft.setTextSize(2);
  tft.print("%");
}
