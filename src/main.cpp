#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

// ===== BOARD-SPECIFIC CONFIGURATION =====
#if defined(BOARD_CYD_RESISTIVE)
  // ESP32-2432S028R (E32R28T) with XPT2046 Resistive Touch
  // Touch uses SEPARATE SPI bus from display!
  // Touch SPI: SCLK=25, MOSI=32, MISO=39, CS=33, IRQ=36
  #include <XPT2046_Touchscreen.h>
  
  #define TOUCH_CS   33
  #define TOUCH_IRQ  36
  #define TOUCH_SCLK 25
  #define TOUCH_MOSI 32
  #define TOUCH_MISO 39
  #define TFT_BACKLIGHT 21
  #define BOARD_NAME "ESP32-2432S028R (Resistive)"
  
  // Touch calibration values (adjust for your specific board)
  #define TOUCH_MIN_X 300
  #define TOUCH_MAX_X 3900
  #define TOUCH_MIN_Y 300
  #define TOUCH_MAX_Y 3900
  
  // Create second SPI bus for touch
  SPIClass touchSPI(HSPI);
  XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

#elif defined(BOARD_CYD_CAPACITIVE)
  // JC2432W328C (Guition) with CST816S Capacitive Touch (I2C)
  // Official pin config from: https://github.com/rzeldent/platformio-espressif32-sunton
  #include <Wire.h>
  #define TOUCH_SDA 33
  #define TOUCH_SCL 32
  #define TOUCH_INT 21   // Note: NOT 36!
  #define TOUCH_RST 25
  #define TFT_BACKLIGHT 27
  #define CST816S_ADDR 0x15
  #define BOARD_NAME "JC2432W328C (Capacitive)"

#else
  #error "No board defined! Use -DBOARD_CYD_RESISTIVE or -DBOARD_CYD_CAPACITIVE"
#endif

// ===== CONFIGURATION =====
// WiFi credentials - UPDATE THESE
const char* WIFI_SSID = "Frontier5664";
const char* WIFI_PASSWORD = "8854950591";

// NTP settings
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = -6 * 3600;  // CST (UTC-6) - adjust for your timezone
const int DAYLIGHT_OFFSET_SEC = 0;      // Set to 3600 if DST is active

// Color thresholds (in seconds)
const int THRESHOLD_YELLOW = 12600;  // 3.5 hours (210 minutes)
const int THRESHOLD_GREEN = 14400;   // 4 hours (240 minutes)

// Touch debounce delay (in milliseconds)
const unsigned long TOUCH_DEBOUNCE_MS = 500;  

// ===== GLOBAL OBJECTS =====
TFT_eSPI tft = TFT_eSPI();

Preferences preferences;

// ===== COLOR DEFINITIONS =====
#define COLOR_RED     0xF800
#define COLOR_YELLOW  0xFFE0
#define COLOR_GREEN   0x07E0
#define COLOR_WHITE   0xFFFF
#define COLOR_BLACK   0x0000

// ===== STATE VARIABLES =====
enum TimerState {
  WAITING_TO_START,
  RUNNING,
  VIEWING_LOGS
};

TimerState currentState = WAITING_TO_START;
TimerState stateBeforeLogs = WAITING_TO_START;  // Track state to return to after logs
unsigned long timerStartMillis = 0;
unsigned long lastTouchMillis = 0;
unsigned long lastUpdateMillis = 0;
int lastDisplayedSeconds = -1;
uint16_t lastBgColor = COLOR_RED;
bool wifiConnected = false;
String lastClockStr = "";

// Button area for logs (lower right corner)
const int LOG_BTN_X = 250;
const int LOG_BTN_Y = 200;
const int LOG_BTN_W = 70;
const int LOG_BTN_H = 40;

// ===== FUNCTION DECLARATIONS =====
void initializeFileSystem();
void connectWiFi();
void logEntry(const char* message);
String getTimestamp();
String getClockString();
void drawClock(uint16_t bgColor);
void drawTimerDisplay(int hours, int minutes, int seconds, uint16_t bgColor, bool forceFullRedraw = false);
void drawWaitingScreen();
void drawLogsButton(uint16_t bgColor);
void drawLogsScreen();
void clearLogs();
bool readTouch(int &screenX, int &screenY);
void handleTouchAt(int touchX, int touchY);
unsigned long getElapsedSeconds();
uint16_t getBackgroundColor(unsigned long seconds);
void formatTime(unsigned long seconds, int &hours, int &minutes, int &secs);
bool isTouchInLogsButton(int x, int y);
bool isTouchInClearButton(int x, int y);

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("Nigel's Potty Timer - Starting...");
  Serial.println(BOARD_NAME);
  Serial.println("========================================");

  // Initialize backlight pin and turn it on
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);
  Serial.println("Backlight ON");

  // Initialize display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(COLOR_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  Serial.println("Display initialized");

  tft.drawString("Initializing touch...", 160, 120);

  // ===== TOUCH CONTROLLER INITIALIZATION =====
#if defined(BOARD_CYD_RESISTIVE)
  // XPT2046 Resistive Touch - uses SEPARATE SPI bus from display
  Serial.printf("Touch pins: CS=%d, IRQ=%d, SCLK=%d, MOSI=%d, MISO=%d\n", 
                TOUCH_CS, TOUCH_IRQ, TOUCH_SCLK, TOUCH_MOSI, TOUCH_MISO);
  
  // Initialize the separate SPI bus for touch (HSPI)
  touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);  // Match display rotation
  Serial.println("XPT2046 touch controller initialized on HSPI");

#elif defined(BOARD_CYD_CAPACITIVE)
  // CST816S Capacitive Touch (I2C)
  Serial.printf("Touch pins: SDA=%d, SCL=%d, RST=%d, INT=%d\n", 
                TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
  
  // Configure RST pin and perform reset
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(20);
  digitalWrite(TOUCH_RST, HIGH);
  delay(100);  // Wait for CST816S to boot
  Serial.println("Touch controller reset complete");

  // Configure INT pin
  pinMode(TOUCH_INT, INPUT);

  // Initialize I2C for touch on correct pins (SDA=33, SCL=32)
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  delay(50);
  
  // Verify touch controller is present and read info
  Wire.beginTransmission(CST816S_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("CST816S found at 0x15");
    
    // Read chip info
    Wire.beginTransmission(CST816S_ADDR);
    Wire.write(0xA7);  // Chip ID register
    Wire.endTransmission(false);
    Wire.requestFrom(CST816S_ADDR, 3);
    if (Wire.available() >= 3) {
      byte chipId = Wire.read();
      byte projId = Wire.read();
      byte fwVer = Wire.read();
      Serial.printf("  Chip ID: 0x%02X, Project: %d, FW: %d\n", chipId, projId, fwVer);
    }
  } else {
    Serial.println("WARNING: CST816S not found!");
  }
#endif

  Serial.println("Touch controller ready");

  tft.fillScreen(COLOR_BLACK);
  tft.drawString("Connecting to WiFi...", 160, 120);

  // Initialize filesystem
  initializeFileSystem();

  // Connect to WiFi and sync time
  connectWiFi();

  // Log boot entry
  logEntry("Boot");

  // Initialize preferences (for future threshold storage)
  preferences.begin("nigel-timer", false);
  preferences.end();

  // Draw initial waiting screen
  drawWaitingScreen();

  Serial.println("Ready! Waiting for first touch...");
}

// ===== TOUCH READ FUNCTION =====
bool readTouch(int &screenX, int &screenY) {
#if defined(BOARD_CYD_RESISTIVE)
  // XPT2046 Resistive Touch
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    
    // Map raw values to screen coordinates
    screenX = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, 320);
    screenY = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, 240);
    
    // Clamp to screen bounds
    screenX = constrain(screenX, 0, 319);
    screenY = constrain(screenY, 0, 239);
    
    return true;
  }
  return false;

#elif defined(BOARD_CYD_CAPACITIVE)
  // CST816S Capacitive Touch - Direct I2C register reads
  Wire.beginTransmission(CST816S_ADDR);
  Wire.write(0x02);  // Start at finger count register
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  
  Wire.requestFrom(CST816S_ADDR, 5);
  if (Wire.available() >= 5) {
    uint8_t fingers = Wire.read();  // 0x02 - finger count
    uint8_t xh = Wire.read();       // 0x03
    uint8_t xl = Wire.read();       // 0x04
    uint8_t yh = Wire.read();       // 0x05
    uint8_t yl = Wire.read();       // 0x06
    
    if (fingers > 0) {
      uint16_t rawX = ((xh & 0x0F) << 8) | xl;
      uint16_t rawY = ((yh & 0x0F) << 8) | yl;
      
      // Map for landscape rotation (rotation=1)
      screenX = rawY;
      screenY = 240 - rawX;
      
      // Clamp to screen bounds
      screenX = constrain(screenX, 0, 319);
      screenY = constrain(screenY, 0, 239);
      
      return true;
    }
  }
  return false;
#endif
}

// ===== MAIN LOOP =====
void loop() {
  // Poll touch at ~20Hz
  static unsigned long lastTouchRead = 0;
  static bool wasTouched = false;
  
  if (millis() - lastTouchRead > 50) {
    lastTouchRead = millis();
    
    int screenX, screenY;
    
    if (readTouch(screenX, screenY)) {
      if (!wasTouched) {  // New touch started
        wasTouched = true;
        
        Serial.printf("TOUCH: screen(%d,%d)\n", screenX, screenY);
        
        unsigned long currentMillis = millis();
        if (currentMillis - lastTouchMillis >= TOUCH_DEBOUNCE_MS) {
          // Process touch at (screenX, screenY)
          handleTouchAt(screenX, screenY);
          lastTouchMillis = currentMillis;
        }
      }
    } else {
      wasTouched = false;
    }
  }
  
  // Update display if timer is running
  if (currentState == RUNNING) {
    unsigned long currentMillis = millis();
    
    // Update every second
    if (currentMillis - lastUpdateMillis >= 1000) {
      lastUpdateMillis = currentMillis;
      
      unsigned long elapsedSeconds = getElapsedSeconds();
      int hours, minutes, seconds;
      formatTime(elapsedSeconds, hours, minutes, seconds);
      
      // Only redraw if seconds changed
      if (elapsedSeconds != lastDisplayedSeconds) {
        uint16_t bgColor = getBackgroundColor(elapsedSeconds);
        drawTimerDisplay(hours, minutes, seconds, bgColor);
        lastDisplayedSeconds = elapsedSeconds;
      }
    }
  }
  
  delay(50);  // Small delay to prevent tight loop
}

// ===== IMPLEMENTATION =====

void initializeFileSystem() {
  if (!LittleFS.begin(true)) {
    Serial.println("ERROR: LittleFS mount failed!");
    tft.fillScreen(COLOR_RED);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 100);
    tft.println("FS ERROR!");
    while (1) delay(1000);
  }
  Serial.println("LittleFS mounted successfully");
}

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // Configure NTP
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    Serial.println("NTP configured, waiting for time sync...");

    // Wait for time to sync (up to 5 seconds)
    struct tm timeinfo;
    int syncAttempts = 0;
    while (!getLocalTime(&timeinfo) && syncAttempts < 10) {
      delay(500);
      syncAttempts++;
    }

    if (getLocalTime(&timeinfo)) {
      Serial.println("Time synchronized!");
      Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      Serial.println("Failed to sync time");
    }
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi connection failed!");
  }
}

String getTimestamp() {
  if (!wifiConnected) {
    return String(millis()) + "ms";
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String(millis()) + "ms";
  }

  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%m/%d/%y %I:%M %p", &timeinfo);
  return String(timestamp);
}

String getClockString() {
  if (!wifiConnected) {
    return "No WiFi";
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "No Time";
  }

  char clockStr[16];
  strftime(clockStr, sizeof(clockStr), "%I:%M%p", &timeinfo);
  return String(clockStr);
}

void drawClock(uint16_t bgColor) {
  // Draw clock in lower left corner
  uint16_t textColor = (bgColor == COLOR_YELLOW) ? COLOR_BLACK : COLOR_WHITE;

  // Clear clock area
  tft.fillRect(0, 210, 100, 30, bgColor);

  String clockStr = getClockString();
  tft.setTextColor(textColor);
  tft.setTextDatum(BL_DATUM);
  tft.setTextSize(2);
  tft.drawString(clockStr.c_str(), 5, 235);
}

void logEntry(const char* message) {
  fs::File logFile = LittleFS.open("/logs.txt", "a");
  if (!logFile) {
    Serial.println("ERROR: Failed to open log file!");
    return;
  }

  String timestamp = getTimestamp();
  char logLine[128];
  snprintf(logLine, sizeof(logLine), "%s %s", timestamp.c_str(), message);

  logFile.println(logLine);
  logFile.close();

  Serial.print("LOG: ");
  Serial.println(logLine);
}

void drawWaitingScreen() {
  tft.fillScreen(COLOR_RED);
  lastBgColor = COLOR_RED;

  // Draw title at top
  tft.setTextColor(COLOR_WHITE);
  tft.setTextDatum(TC_DATUM);  // Top center
  tft.setTextSize(3);
  tft.drawString("Nigel Timer!", 160, 20);

  // Draw "Touch to Start" message
  tft.setTextDatum(MC_DATUM);  // Middle center
  tft.setTextSize(2);
  tft.drawString("Touch to Start", 160, 100);

  // Draw timer at 0:00:00 with whitespace above
  tft.setTextSize(4);
  tft.drawString("00:00:00", 160, 170);

  // Draw clock in lower left
  drawClock(COLOR_RED);

  // Draw logs button
  drawLogsButton(COLOR_RED);
}

void drawLogsButton(uint16_t bgColor) {
  // Draw a small "Logs" button in lower right corner
  uint16_t btnColor = (bgColor == COLOR_YELLOW) ? COLOR_BLACK : COLOR_WHITE;
  tft.drawRect(LOG_BTN_X, LOG_BTN_Y, LOG_BTN_W, LOG_BTN_H, btnColor);
  tft.setTextColor(btnColor);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.drawString("LOGS", LOG_BTN_X + LOG_BTN_W/2, LOG_BTN_Y + LOG_BTN_H/2);
}

bool isTouchInLogsButton(int x, int y) {
  return (x >= LOG_BTN_X && x <= LOG_BTN_X + LOG_BTN_W &&
          y >= LOG_BTN_Y && y <= LOG_BTN_Y + LOG_BTN_H);
}

// Clear button area (lower left corner)
const int CLEAR_BTN_X = 10;
const int CLEAR_BTN_Y = 200;
const int CLEAR_BTN_W = 70;
const int CLEAR_BTN_H = 30;

bool isTouchInClearButton(int x, int y) {
  return (x >= CLEAR_BTN_X && x <= CLEAR_BTN_X + CLEAR_BTN_W &&
          y >= CLEAR_BTN_Y && y <= CLEAR_BTN_Y + CLEAR_BTN_H);
}

void clearLogs() {
  LittleFS.remove("/logs.txt");
  Serial.println("Logs cleared!");
}

void drawLogsScreen() {
  tft.fillScreen(COLOR_BLACK);

  // Title
  tft.setTextColor(COLOR_WHITE);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Recent Logs", 160, 10);

  // Read and display logs (most recent first)
  fs::File logFile = LittleFS.open("/logs.txt", "r");
  if (!logFile) {
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No logs found", 160, 120);
    // Still draw clear button and footer
  }

  else {
    // Read ALL lines, keeping only the last MAX_DISPLAY in a circular buffer
    const int MAX_DISPLAY = 9;
    String lines[MAX_DISPLAY];
    int writeIdx = 0;
    int totalLines = 0;

    while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      line.trim();  // Remove \r and whitespace
      lines[writeIdx] = line;
      writeIdx = (writeIdx + 1) % MAX_DISPLAY;
      totalLines++;
    }
    logFile.close();

    // Display lines (most recent first)
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    int yPos = 40;
    int numToShow = min(totalLines, MAX_DISPLAY);

    // Start from most recent (one before writeIdx) and go backwards
    for (int i = 0; i < numToShow; i++) {
      int idx = (writeIdx - 1 - i + MAX_DISPLAY) % MAX_DISPLAY;
      tft.drawString(lines[idx].c_str(), 10, yPos);
      yPos += 18;
    }
  }

  // Clear button (lower left)
  tft.drawRect(CLEAR_BTN_X, CLEAR_BTN_Y, CLEAR_BTN_W, CLEAR_BTN_H, COLOR_RED);
  tft.setTextColor(COLOR_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.drawString("CLEAR", CLEAR_BTN_X + CLEAR_BTN_W/2, CLEAR_BTN_Y + CLEAR_BTN_H/2);

  // Footer instruction
  tft.setTextDatum(BC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_YELLOW);
  tft.drawString("Touch anywhere to return", 160, 235);
}

void drawTimerDisplay(int hours, int minutes, int seconds, uint16_t bgColor, bool forceFullRedraw) {
  // Choose text color based on background (black on yellow for readability)
  uint16_t textColor = (bgColor == COLOR_YELLOW) ? COLOR_BLACK : COLOR_WHITE;

  // Only redraw full screen if background color changed or forced
  if (forceFullRedraw || bgColor != lastBgColor) {
    tft.fillScreen(bgColor);
    lastBgColor = bgColor;

    // Draw title at top
    tft.setTextColor(textColor);
    tft.setTextDatum(TC_DATUM);  // Top center
    tft.setTextSize(3);
    tft.drawString("Nigel Timer!", 160, 20);

    // Draw logs button
    drawLogsButton(bgColor);

    // Draw clock
    drawClock(bgColor);
    lastClockStr = getClockString();
  } else {
    // Just clear the timer area (approximate size of text)
    tft.fillRect(40, 130, 240, 50, bgColor);

    // Update clock if minute changed
    String currentClock = getClockString();
    if (currentClock != lastClockStr) {
      drawClock(bgColor);
      lastClockStr = currentClock;
    }
  }

  // Format time string
  char timeStr[16];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hours, minutes, seconds);

  // Draw time below title with whitespace
  tft.setTextColor(textColor);
  tft.setTextDatum(MC_DATUM);  // Middle center
  tft.setTextSize(4);
  tft.drawString(timeStr, 160, 150);
}

void handleTouch() {
  // Legacy function - no longer used with direct register reads
  Serial.println("handleTouch() called - should use handleTouchAt() instead");
}

void handleTouchAt(int touchX, int touchY) {
  Serial.printf("Processing touch at: %d, %d\n", touchX, touchY);

  if (currentState == VIEWING_LOGS) {
    // Check if clear button was pressed
    if (isTouchInClearButton(touchX, touchY)) {
      Serial.println("Clear logs button pressed");
      clearLogs();
      drawLogsScreen();  // Redraw to show empty logs
      return;
    }

    // Any other touch returns to previous state
    Serial.println("Returning from logs");
    currentState = stateBeforeLogs;
    lastBgColor = 0;  // Force full redraw

    if (currentState == WAITING_TO_START) {
      drawWaitingScreen();
    } else {
      unsigned long elapsedSeconds = getElapsedSeconds();
      int hours, minutes, seconds;
      formatTime(elapsedSeconds, hours, minutes, seconds);
      drawTimerDisplay(hours, minutes, seconds, getBackgroundColor(elapsedSeconds), true);
    }
    return;
  }

  // Check if logs button was pressed (works from waiting or running state)
  if ((currentState == RUNNING || currentState == WAITING_TO_START) && isTouchInLogsButton(touchX, touchY)) {
    Serial.println("Logs button pressed");
    stateBeforeLogs = currentState;  // Remember where we came from
    currentState = VIEWING_LOGS;
    drawLogsScreen();
    return;
  }

  if (currentState == WAITING_TO_START) {
    // First touch (not on logs button) - start the timer
    Serial.println("Timer started!");
    currentState = RUNNING;
    timerStartMillis = millis();
    lastUpdateMillis = millis();
    lastDisplayedSeconds = 0;

    // Draw initial running display (force full redraw)
    drawTimerDisplay(0, 0, 0, COLOR_RED, true);

  } else if (currentState == RUNNING) {
    // Subsequent touch (not on logs button) - log duration and reset
    unsigned long elapsedSeconds = getElapsedSeconds();
    int hours, minutes, seconds;
    formatTime(elapsedSeconds, hours, minutes, seconds);

    // Log the duration
    char logMessage[64];
    snprintf(logMessage, sizeof(logMessage), "-- Duration: %02d:%02d:%02d", hours, minutes, seconds);
    logEntry(logMessage);

    Serial.print("Timer reset! Previous duration: ");
    Serial.println(logMessage);

    // Reset timer
    timerStartMillis = millis();
    lastUpdateMillis = millis();
    lastDisplayedSeconds = 0;

    // Redraw with red background (force full redraw)
    drawTimerDisplay(0, 0, 0, COLOR_RED, true);
  }
}

unsigned long getElapsedSeconds() {
  if (currentState != RUNNING) {
    return 0;
  }
  return (millis() - timerStartMillis) / 1000;
}

uint16_t getBackgroundColor(unsigned long seconds) {
  if (seconds < THRESHOLD_YELLOW) {
    return COLOR_RED;
  } else if (seconds < THRESHOLD_GREEN) {
    return COLOR_YELLOW;
  } else {
    return COLOR_GREEN;
  }
}

void formatTime(unsigned long totalSeconds, int &hours, int &minutes, int &secs) {
  hours = totalSeconds / 3600;
  minutes = (totalSeconds % 3600) / 60;
  secs = totalSeconds % 60;
}
