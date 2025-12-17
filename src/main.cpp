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
const unsigned long TOUCH_DEBOUNCE_MS = 2000;

// ===== GLOBAL OBJECTS =====
TFT_eSPI tft = TFT_eSPI();

// Create separate SPI instance for touch controller
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

Preferences preferences;

// ===== STATE VARIABLES =====
enum TimerState {
  WAITING_TO_START,
  RUNNING
};

TimerState currentState = WAITING_TO_START;
unsigned long timerStartMillis = 0;
unsigned long lastTouchMillis = 0;
unsigned long lastUpdateMillis = 0;
int lastDisplayedSeconds = -1;

// ===== COLOR DEFINITIONS =====
#define COLOR_RED     0xF800
#define COLOR_YELLOW  0xFFE0
#define COLOR_GREEN   0x07E0
#define COLOR_WHITE   0xFFFF
#define COLOR_BLACK   0x0000

// ===== FUNCTION DECLARATIONS =====
void initializeFileSystem();
void logEntry(const char* message);
void drawTimerDisplay(int hours, int minutes, int seconds, uint16_t bgColor);
void drawWaitingScreen();
void handleTouch();
unsigned long getElapsedSeconds();
uint16_t getBackgroundColor(unsigned long seconds);
void formatTime(unsigned long seconds, int &hours, int &minutes, int &secs);

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
}

void drawTimerDisplay(int hours, int minutes, int seconds, uint16_t bgColor) {
  tft.fillScreen(bgColor);

  // Draw title at top
  tft.setTextColor(COLOR_WHITE);
  tft.setTextDatum(TC_DATUM);  // Top center
  tft.setTextSize(3);
  tft.drawString("Nigel Timer!", 160, 20);

  // Format time string
  char timeStr[16];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hours, minutes, seconds);

  // Draw time below title with whitespace
  tft.setTextDatum(MC_DATUM);  // Middle center
  tft.setTextSize(4);
  tft.drawString(timeStr, 160, 150);
}

void handleTouch() {
  if (currentState == WAITING_TO_START) {
    // First touch - start the timer
    Serial.println("Timer started!");
    currentState = RUNNING;
    timerStartMillis = millis();
    lastUpdateMillis = millis();
    lastDisplayedSeconds = 0;
    
    // Draw initial running display
    drawTimerDisplay(0, 0, 0, COLOR_RED);
    
  } else if (currentState == RUNNING) {
    // Subsequent touch - log duration and reset
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
    
    // Redraw with red background
    drawTimerDisplay(0, 0, 0, COLOR_RED);
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
