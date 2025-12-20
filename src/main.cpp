#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>

// ===== PIN DEFINITIONS =====
// Touch Controller Pins (separate SPI bus from LCD)
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_CLK 25
#define TOUCH_DIN 32  // MOSI
#define TOUCH_DO 39   // MISO

// Backlight
#define TFT_BACKLIGHT 21

// ===== CONFIGURATION =====
// Color thresholds (in seconds)
const int THRESHOLD_YELLOW = 12600;  // 3.5 hours (210 minutes)
const int THRESHOLD_GREEN = 14400;   // 4 hours (240 minutes)

// Touch debounce delay (in milliseconds)
const unsigned long TOUCH_DEBOUNCE_MS = 1000;

// ===== GLOBAL OBJECTS =====
TFT_eSPI tft = TFT_eSPI();

// Create separate SPI instance for touch controller
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

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

// Button area for logs (lower right corner)
const int LOG_BTN_X = 250;
const int LOG_BTN_Y = 200;
const int LOG_BTN_W = 70;
const int LOG_BTN_H = 40;

// ===== FUNCTION DECLARATIONS =====
void initializeFileSystem();
void logEntry(const char* message);
void drawTimerDisplay(int hours, int minutes, int seconds, uint16_t bgColor, bool forceFullRedraw = false);
void drawWaitingScreen();
void drawLogsButton(uint16_t bgColor);
void drawLogsScreen();
void handleTouch();
unsigned long getElapsedSeconds();
uint16_t getBackgroundColor(unsigned long seconds);
void formatTime(unsigned long seconds, int &hours, int &minutes, int &secs);
bool isTouchInLogsButton(int x, int y);

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("Nigel's Potty Timer - Starting...");

  // Initialize backlight pin and turn it on
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);
  Serial.println("Backlight ON");

  // Initialize display
  tft.init();
  tft.setRotation(1);  // Landscape mode (320x240)
  tft.fillScreen(COLOR_BLACK);
  Serial.println("Display initialized");

  // Initialize touch SPI on custom pins
  touchSPI.begin(TOUCH_CLK, TOUCH_DO, TOUCH_DIN, TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);  // Match display rotation
  Serial.println("Touch initialized");

  // Initialize filesystem
  initializeFileSystem();

  // Log boot entry
  logEntry("Boot");

  // Initialize preferences (for future threshold storage)
  preferences.begin("nigel-timer", false);
  preferences.end();

  // Draw initial waiting screen
  drawWaitingScreen();

  Serial.println("Ready! Waiting for first touch...");
}

// ===== MAIN LOOP =====
void loop() {
  // Check for touch input
  if (touch.tirqTouched() && touch.touched()) {
    unsigned long currentMillis = millis();
    
    // Debounce check
    if (currentMillis - lastTouchMillis >= TOUCH_DEBOUNCE_MS) {
      handleTouch();
      lastTouchMillis = currentMillis;
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

void logEntry(const char* message) {
  fs::File logFile = LittleFS.open("/logs.txt", "a");
  if (!logFile) {
    Serial.println("ERROR: Failed to open log file!");
    return;
  }
  
  char logLine[128];
  unsigned long uptime = millis();
  snprintf(logLine, sizeof(logLine), "%s at +%lums", message, uptime);
  
  logFile.println(logLine);
  logFile.close();
  
  Serial.print("LOG: ");
  Serial.println(logLine);
}

void drawWaitingScreen() {
  tft.fillScreen(COLOR_RED);

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
    return;
  }

  // Read all lines into array (we'll display last ones)
  const int MAX_LINES = 20;
  String lines[MAX_LINES];
  int lineCount = 0;

  while (logFile.available() && lineCount < MAX_LINES) {
    lines[lineCount] = logFile.readStringUntil('\n');
    lineCount++;
  }
  logFile.close();

  // Display from most recent (bottom of file) going up
  // Fit about 10 lines on screen at size 1
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  int yPos = 40;
  int maxDisplayLines = 10;
  int startIdx = (lineCount > maxDisplayLines) ? lineCount - maxDisplayLines : 0;

  for (int i = lineCount - 1; i >= startIdx && yPos < 230; i--) {
    tft.drawString(lines[i].c_str(), 10, yPos);
    yPos += 18;
  }

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
  } else {
    // Just clear the timer area (approximate size of text)
    tft.fillRect(40, 130, 240, 50, bgColor);
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
  // Get touch coordinates
  TS_Point p = touch.getPoint();

  // Map touch coordinates to screen (adjust based on rotation)
  // XPT2046 returns values 0-4095, map to screen size 320x240
  int touchX = map(p.x, 200, 3900, 0, 320);
  int touchY = map(p.y, 200, 3900, 0, 240);

  Serial.printf("Touch at: %d, %d (raw: %d, %d)\n", touchX, touchY, p.x, p.y);

  if (currentState == VIEWING_LOGS) {
    // Any touch returns to previous state
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
    snprintf(logMessage, sizeof(logMessage), "Duration: %02d:%02d:%02d", hours, minutes, seconds);
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
