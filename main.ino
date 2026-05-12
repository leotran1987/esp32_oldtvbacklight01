#include <WiFi.h>
#include "time.h"
#include <Adafruit_GFX.h>    
#include <Adafruit_ST7789.h> 
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <HTTPClient.h>

#include "config.h"
#include <WiFiMulti.h>

// Initialize WiFiMulti for multiple AP management
WiFiMulti wifiMulti;

// NTP Server for time synchronization
const char* ntpServer = "pool.ntp.org";

// Weather data variables
String weatherRaw = "";
unsigned long lastWeatherUpdate = 0; 
const unsigned long weatherInterval = 1800000; // Update every 30 minutes (30 * 60 * 1000 ms)

// Pin definitions for ESP32-C3 and TFT/Touch/Relay
#define TFT_CS 7
#define TFT_DC 3
#define TFT_RST 10
#define T_CS 1
#define T_IRQ 2
#define RELAY_PIN 8
#define SD_CS 0

// Hardware instances
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(T_CS, T_IRQ);

// Theme Colors (Cyberpunk/Amber aesthetic)
#define COLOR_AMBER 0xFD00  
#define COLOR_BG    0x0000  
#define COLOR_OFF   0x8200 // Dim Amber for background elements

// 1. Inspiration Quotes List
const char* quotes[] = {
  "Stay hungry, stay foolish - Steve Jobs",
  "Imagination is more important than knowledge - Albert Einstein",
  "The only way to do great work is to love what you do - Steve Jobs",
  "Done is better than perfect - Sheryl Sandberg",
  "Innovation distinguishes between a leader and a follower - Steve Jobs",
  "Be the change that you wish to see in the world - Mahatma Gandhi",
  "Your time is limited, so don't waste it - Steve Jobs",
  "Stay positive, work hard, make it happen - Alex Dam",
  "Life is what happens when you're busy making plans - John Lennon",
  "The best way to predict the future is to create it - Peter Drucker"
};

const int totalQuotes = sizeof(quotes) / sizeof(quotes[0]);
int quoteOrder[totalQuotes]; // Array to store shuffled indices
int currentQuoteIndex = 0;    // Current position in shuffled sequence
unsigned long lastQuoteTime = 0;

// Fisher-Yates shuffle algorithm for randomizing quotes
void shuffleQuotes() {
  for (int i = 0; i < totalQuotes; i++) quoteOrder[i] = i; // Reset indices
  for (int i = totalQuotes - 1; i > 0; i--) {
    int j = random(0, i + 1);
    int temp = quoteOrder[i];
    quoteOrder[i] = quoteOrder[j];
    quoteOrder[j] = temp;
  }
  currentQuoteIndex = 0; // Reset to start of new shuffled sequence
}

bool relayState = false;
unsigned long lastClockUpdate = 0;
int lastMin = -1;
int lastDay = -1; 

/**
 * 2. Draws center-aligned text with smart word wrapping and background clearing
 * Used for displaying inspiration quotes at the bottom of the screen
 */
void drawCenterWrappedText(String rawText, int x_center, int y_start, int line_width) {
  tft.setTextSize(1);
  tft.setTextColor(COLOR_AMBER);
  
  // Clear quote area (y=210 to y=248)
  tft.fillRect(5, 210, 230, 38, COLOR_BG);

  int dashIndex = rawText.indexOf(" - ");
  String quoteContent = "";
  String authorName = "";

  // Split quote and author
  if (dashIndex != -1) {
    quoteContent = "\"" + rawText.substring(0, dashIndex) + "\"";
    authorName = "- " + rawText.substring(dashIndex + 3) + " -";
  } else {
    quoteContent = "\"" + rawText + "\"";
  }

  // --- PHASE 1: DRAW QUOTE (AUTO WORD WRAP) ---
  int16_t x1, y1; uint16_t w, h;
  int cursorY = y_start;
  String currentLine = "";
  String word = "";
  
  for (int i = 0; i <= quoteContent.length(); i++) {
    if (i == quoteContent.length() || quoteContent[i] == ' ') {
      String testLine = currentLine + (currentLine == "" ? "" : " ") + word;
      tft.getTextBounds(testLine, 0, 0, &x1, &y1, &w, &h);

      if (w > line_width) {
        tft.getTextBounds(currentLine, 0, 0, &x1, &y1, &w, &h);
        tft.setCursor(x_center - (w / 2), cursorY);
        tft.print(currentLine);
        currentLine = word;
        cursorY += 10; 
      } else {
        currentLine = testLine;
      }
      word = "";
    } else {
      word += quoteContent[i];
    }
  }
  
  // Draw last line of the quote
  tft.getTextBounds(currentLine, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(x_center - (w / 2), cursorY);
  tft.print(currentLine);

  // --- PHASE 2: DRAW AUTHOR (ALWAYS ON NEW LINE) ---
  if (authorName != "") {
    cursorY += 12; // Extra line spacing for author
    tft.getTextBounds(authorName, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(x_center - (w / 2), cursorY);
    tft.print(authorName);
  }
}

// --- DRAW BATTERY ICON ---
void drawBatIcon(int x, int y, int percent) {
  tft.drawRect(x, y, 20, 10, COLOR_AMBER); // Battery body
  tft.fillRect(x + 20, y + 2, 2, 6, COLOR_AMBER); // Battery tip
  int w = map(percent, 0, 100, 0, 16);
  tft.fillRect(x + 2, y + 2, 16, 6, COLOR_BG); // Clear inside
  tft.fillRect(x + 2, y + 2, w, 6, COLOR_AMBER); // Fill level
}

// --- DRAW WIFI RSSI ICON ---
void drawWifiIcon(int x, int y) {
  long rssi = WiFi.RSSI();
  // Signal level logic
  int bars = (rssi > -50) ? 5 : (rssi > -60) ? 4 : (rssi > -70) ? 3 : (rssi > -80) ? 2 : (rssi > -90) ? 1 : 0;
  for (int i = 0; i < 5; i++) {
    int h = (i + 1) * 3;
    uint16_t color = (i < bars) ? COLOR_AMBER : COLOR_OFF;
    tft.fillRect(x + (i * 4), y + (15 - h), 3, h, color);
  }
}

// --- DRAW TEMPERATURE DEGREE SYMBOL ---
void drawTempIcon(int x, int y) {
  tft.drawCircle(x, y, 2, COLOR_AMBER);
}

// --- UI COMPONENT: VIRTUAL TOUCH BUTTON ---
void updateButton() {
  // 1. Define safety clear zone
  int bx = 5, by = 255, bw = 230, bh = 60; 
  tft.fillRect(bx, by, bw, bh, COLOR_BG); // Clear previous state

  // Internal drawing coordinates
  int nx = 10, ny = 260, nw = 220, nh = 50;

  if (relayState) {
    // --- ACTIVE STATE (Solid Fill) ---
    tft.fillRoundRect(nx + 5, ny + 5, nw - 10, nh - 10, 2, COLOR_AMBER);
    tft.setTextColor(COLOR_BG); 
    tft.setTextSize(2);
    
    String txt = ">> SYSTEM ACTIVE <<";
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((240 - w) / 2, ny + 18);
    tft.print(txt);
  } else {
    // --- READY STATE (Outline Frame) ---
    tft.setTextColor(0x8200); 
    int len = 15; // Length of corner brackets
    for(int i = 0; i < 3; i++) { 
      int d = i;
      // Draw corner aesthetic brackets
      tft.drawFastHLine(nx + d, ny + d, len, 0x8200); 
      tft.drawFastVLine(nx + d, ny + d, len, 0x8200); 
      tft.drawFastHLine(nx + nw - len - d, ny + d, len, 0x8200); 
      tft.drawFastVLine(nx + nw - d, ny + d, len, 0x8200);
      tft.drawFastHLine(nx + d, ny + nh - d, len, 0x8200); 
      tft.drawFastVLine(nx + d, ny + nh - len - d, len, 0x8200);
      tft.drawFastHLine(nx + nw - len - d, ny + nh - d, len, 0x8200); 
      tft.drawFastVLine(nx + nw - d, ny + nh - len - d, len, 0x8200);
    }

    tft.setTextSize(2);
    String txt = "[ SYSTEM READY ]";
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((240 - w) / 2, ny + 18);
    tft.print(txt);
    
    // Side decorative bars
    tft.fillRect(nx + 18, ny + 18, 3, 14, 0x8200);
    tft.fillRect(nx + nw - 21, ny + 18, 3, 14, 0x8200);
  }
}

void setup() {
  Serial.begin(115200);
  // I/O Initialization
  pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, LOW);
  pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH); // Disable SD SPI for stability

  // Screen Initialization
  tft.init(240, 320);
  tft.setRotation(2);
  tft.invertDisplay(false); 
  tft.fillScreen(COLOR_BG);

  // START BOOT ANIMATION (Hacker Style)
  playBootSequence();

  // Draw basic UI layout
  drawStaticGrid();

  // --- WIFI CONNECTION WITH TIMEOUT ---
  // Add multiple APs from config.h for redundancy
  wifiMulti.addAP(MY_SSID, MY_PASSWORD);
  wifiMulti.addAP(MY_SSID_2, MY_PASSWORD_2);
  wifiMulti.addAP(MY_SSID_3, MY_PASSWORD_3);

  Serial.println("Connecting WiFi...");
  unsigned long startAttemptTime = millis();
  
  // Wait for connection (max 10 seconds)
  while (wifiMulti.run() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    updateWeather(); // Fetch real-time weather if online
  } else {
    Serial.println("\nWiFi Timeout - Starting Offline Mode");
    weatherRaw = "N/A|N/A|N/A|N/A"; // Placeholder values for offline display
  }

  // Display weather information
  drawWeatherInfo();
  lastWeatherUpdate = millis(); // Set timer for next sync

  // Configure Time and Touchscreen
  configTime(7 * 3600, 0, ntpServer); // UTC+7 for Vietnam
  ts.begin(); ts.setRotation(2);

  randomSeed(analogRead(0)); // Seed random for quote shuffling
  shuffleQuotes();           // Initial quote order

  updateButton(); // Initial button render
}

/**
 * EYE-CANDY: "Hacker" style terminal boot sequence
 * Simulates system breach for cinematic effect
 */
void playBootSequence() {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_AMBER);
  tft.setTextSize(1);
  
  String prompt = "root@sparkworks:~# ";
  tft.drawFastHLine(0, 5, 240, COLOR_OFF); // Decorative separator

  // 2. Command line simulation
  tft.setCursor(5, 12); tft.print(prompt + "mounting /dev/esp32..."); delay(300);
  tft.setCursor(5, 22); tft.print(prompt + "loading kernel... ok"); delay(250);
  tft.setCursor(5, 32); tft.print(prompt + "starting auth_service..."); delay(200);

  // 3. Password "Cracking" effect
  const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789!@#$%";
  char pass[13]; 
  pass[12] = '\0';
  int yPass = 45;
  for (int round = 0; round < 35; round++) {
    tft.fillRect(90, yPass, 120, 10, COLOR_BG); // Localized text clearing
    tft.setCursor(5, yPass);
    tft.print("cracking: [ ");
    
    for (int i = 0; i < 12; i++) {
      if (round < 15 + i) {
        pass[i] = chars[random(0, sizeof(chars)-1)];
      } else {
        const char realPass[] = "gateway_2026"; 
        pass[i] = realPass[i];
      }
    }
    tft.print(pass); tft.print(" ]");
    delay(15 + round); 
  }
  
  tft.setCursor(5, 60); tft.print(">> status: handshake_ok"); delay(300);
  tft.setCursor(5, 70); tft.print(">> bypass: completed"); delay(250);

  // 4. Network port scanning simulation
  tft.drawFastHLine(0, 85, 240, COLOR_OFF);
  tft.setCursor(5, 95); tft.print(prompt + "nmap -p 80,443 -sS"); delay(200);

  int yPort = 110;
  tft.drawRect(10, yPort, 220, 10, COLOR_OFF); // Scanning bar border
  for (int w = 0; w < 210; w += 7) {
    tft.fillRect(15 + w, yPort + 2, 4, 6, COLOR_AMBER);
    tft.fillRect(150, yPort + 15, 60, 10, COLOR_BG); 
    tft.setCursor(5, yPort + 15);
    tft.print("scanning_ports: "); 
    tft.print(map(w, 0, 203, 0, 100)); tft.print("%");
    delay(random(10, 50)); 
  }
  
  tft.setCursor(5, 145); tft.print(">> open: [80/http] vulnerable"); delay(400);
  tft.setCursor(5, 155); tft.print(prompt + "injecting remote_shell..."); delay(200);
  
  // 5. GLITCH EFFECT (Text flashing)
  for(int i = 0; i < 4; i++) {
    tft.setTextColor(COLOR_BG);
    tft.setCursor(5, 170); tft.print(">> bash_session: stabilized"); delay(30);
    tft.setTextColor(COLOR_AMBER);
    tft.setCursor(5, 170); tft.print(">> bash_session: stabilized"); delay(30);
  }
  
  tft.setCursor(5, 170); tft.print(">> bash_session: stabilized"); delay(300);
  tft.setCursor(5, 180); tft.print(prompt + "sh wipe_syslogs.sh"); delay(200);
  
  // 6. Rapid log cleaning simulation
  for(int i = 0; i < 6; i++) {
    tft.setCursor(5, 195 + (i * 12));
    tft.print("] clear_entry: "); 
    tft.print(random(1000, 9999)); tft.print(".log cleaned.");
    delay(40);
  }
  
  // 7. FINAL BOOT LOGO
  delay(300);
  tft.setCursor(5, 270); 
  tft.setTextSize(2); 
  tft.print("BREACH SUCCESS."); 
  delay(500);
}

/**
 * Draws the static UI skeleton (Grid lines and labels)
 */
void drawStaticGrid() {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_AMBER);
  tft.setTextSize(1);

  // 1. Horizontal Dividers
  tft.drawFastHLine(0, 5, 240, COLOR_AMBER);   
  tft.drawFastHLine(0, 45, 240, COLOR_AMBER);  
  tft.drawFastHLine(0, 110, 240, COLOR_AMBER); 
  tft.drawFastHLine(0, 205, 240, COLOR_AMBER); 
  tft.drawFastHLine(0, 250, 240, COLOR_AMBER); 

  // 2. Decorative Clock Brackets
  int yT = 115, yB = 200;
  tft.drawFastVLine(5, yT, 15, COLOR_AMBER); tft.drawFastHLine(5, yT, 15, COLOR_AMBER); 
  tft.drawFastVLine(234, yT, 15, COLOR_AMBER); tft.drawFastHLine(220, yT, 15, COLOR_AMBER); 
  tft.drawFastVLine(5, yB-15, 15, COLOR_AMBER); tft.drawFastHLine(5, yB, 15, COLOR_AMBER); 
  tft.drawFastVLine(234, yB-15, 15, COLOR_AMBER); tft.drawFastHLine(220, yB, 15, COLOR_AMBER); 

  // 3. Aesthetic decorations
  tft.setCursor(10, 32); tft.print("+");
  tft.setCursor(225, 32); tft.print("+");
  
  tft.setCursor(85, 21); tft.print("New York City");
  tft.setCursor(210, 21); tft.print("OK");
}

/**
 * Dynamic content update (Clock, Date, Quotes)
 * Runs every loop but internal timers limit actual screen writing
 */
void updateDisplay() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return;

  // QUOTE ROTATION: Every 2 seconds
  if (millis() - lastQuoteTime >= 2000) {
    lastQuoteTime = millis();
    int quoteIdx = quoteOrder[currentQuoteIndex];
    drawCenterWrappedText(quotes[quoteIdx], 120, 215, 210);
    
    currentQuoteIndex++;
    if (currentQuoteIndex >= totalQuotes) {
      shuffleQuotes(); // Reshuffle when all quotes have been shown
    }
  }

  // CLOCK REFRESH: Every 1 second
  if (millis() - lastClockUpdate >= 1000) {
    lastClockUpdate = millis();
    tft.setTextColor(COLOR_AMBER, COLOR_BG);

    // Update Battery and WiFi icons every 30 seconds
    if (timeinfo.tm_sec % 30 == 0 || lastMin == -1) {
       drawBatIcon(15, 20, 85);
       tft.setTextSize(1);
       tft.setCursor(40, 21); tft.print("85% "); 
       drawWifiIcon(185, 17);
    }

    // HH:MM Update
    if (timeinfo.tm_min != lastMin) {
      lastMin = timeinfo.tm_min;
      tft.fillRect(15, 125, 180, 50, COLOR_BG); // Clear clock area
      tft.setTextSize(6);
      char hm[6]; strftime(hm, 6, "%H:%M", &timeinfo);
      tft.setCursor(20, 130); tft.print(hm);
    }

    // Seconds Update
    tft.fillRect(195, 130, 35, 20, COLOR_BG);
    tft.setTextSize(2);
    char sec[3]; strftime(sec, 3, "%S", &timeinfo);
    tft.setCursor(200, 130); tft.print(sec);

    // Date Update
    if (timeinfo.tm_mday != lastDay) {
      lastDay = timeinfo.tm_mday;
      tft.fillRect(40, 175, 160, 25, COLOR_BG);
      tft.setTextSize(2);
      char dt[20]; strftime(dt, 20, "%d %b %Y", &timeinfo);
      tft.setCursor(45, 180); tft.print(dt);
    }
  }
}

/**
 * Fetches weather string from wttr.in
 * Format: Temperature | Precip | Humidity | Wind
 */
void updateWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://wttr.in/NewYork?format=%t|%p|%h|%w&m"); 
    
    int httpCode = http.GET();
    if (httpCode == 200) {
      weatherRaw = http.getString();
      weatherRaw.replace("+", ""); // Clean raw string
      Serial.println("NYC Weather: " + weatherRaw);
    }
    http.end();
  }
}

/**
 * Parses raw weather string and renders it to the screen
 * Includes random rain simulation if data is 0.0mm
 */
void drawWeatherInfo() {
  int p1 = weatherRaw.indexOf('|');
  int p2 = weatherRaw.indexOf('|', p1 + 1);
  int p3 = weatherRaw.indexOf('|', p2 + 1);

  if (p1 != -1 && p2 != -1 && p3 != -1) {
    String t = weatherRaw.substring(0, p1);    // Temperature
    String r = weatherRaw.substring(p1 + 1, p2); // Rainfall
    String h = weatherRaw.substring(p2 + 1, p3); // Humidity
    String w = weatherRaw.substring(p3 + 1);    // Wind Speed/Direction

    // RAIN SIMULATION: If 0.0mm, generate random value 0.0 - 20.0 for visual variety
    if (r == "0.0mm") {
      float randomRain = random(0, 201) / 10.0;
      r = String(randomRain, 1) + "mm";
    }

    // Clean data for cleaner display
    t.replace("+", "");
    t.replace("°C", ""); 

    // Filter out Unicode wind arrow characters
    int firstDigit = -1;
    for (int i = 0; i < w.length(); i++) {
      if (isDigit(w[i])) { firstDigit = i; break; }
    }
    if (firstDigit != -1) {
      w = w.substring(firstDigit);
    }

    // CLEAR OLD WEATHER AREA
    tft.fillRect(10, 60, 220, 48, COLOR_BG);
    tft.setTextColor(COLOR_AMBER);

    // DRAW MAIN TEMPERATURE (Large font on left)
    tft.setTextSize(3);
    tft.setCursor(15, 65); tft.print(t);
    
    // Dynamic calculation for degree symbol placement
    int16_t x1, y1; uint16_t w_temp, h_temp;
    tft.getTextBounds(t, 15, 65, &x1, &y1, &w_temp, &h_temp); 
    drawTempIcon(15 + w_temp + 5, 68);
    tft.setCursor(15 + w_temp + 15, 65); tft.print("C");

    // DRAW DETAILS (Small font on right)
    tft.setTextSize(1);
    tft.setCursor(110, 65); tft.print("HUMIDITY: " + h); 
    tft.setCursor(110, 78); tft.print("WIND_SPD: " + w + " kph"); 
    tft.setCursor(110, 91); tft.print("RAIN_VAL: " + r);
    
  } else {
    // Connection failure display
    tft.fillRect(10, 60, 220, 48, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(110, 75); tft.print("SYNC ERROR");
  }
}

void loop() {
  // TASK 1: Update clock and inspiration quotes
  updateDisplay();

  // TASK 2: Periodic weather sync (every 30 mins)
  if (millis() - lastWeatherUpdate >= weatherInterval || lastWeatherUpdate == 0) {
    lastWeatherUpdate = millis();
    Serial.println("System: initiating periodic weather sync...");
    updateWeather();    
    drawWeatherInfo();
  }

  // TASK 3: Handle Touchscreen events (MOSFET toggle)
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    // Map touch coordinates to screen resolution
    int y = map(p.y, 3800, 200, 0, 320);
    if (y > 255) { // Button area detected
      relayState = !relayState;
      digitalWrite(RELAY_PIN, relayState); // Toggle MOSFET/Relay
      updateButton(); // Refresh button UI
      delay(400); // Debounce
    }
  }
}