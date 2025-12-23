# CYD Board Comparison: ESP32-2432S028R vs JC2432W328C

This document describes the hardware differences between two "Cheap Yellow Display" (CYD) ESP32 boards and what configuration changes are required to run the same application on each.

## Official Documentation Sources

| Board | Vendor | Documentation URL |
|-------|--------|-------------------|
| ESP32-2432S028R | Sunton | https://www.lcdwiki.com/2.8inch_ESP32-32E_Display |
| JC2432W328C | Guition | https://github.com/rzeldent/platformio-espressif32-sunton |

## Board Overview

| Feature | ESP32-2432S028R (Original CYD) | JC2432W328C (Capacitive CYD) |
|---------|-------------------------------|------------------------------|
| Display Size | 2.8" 320x240 | 2.8" 320x240 |
| Display Driver | **ILI9341** | **ST7789** |
| Display Interface | SPI | SPI |
| Touch Type | **Resistive (XPT2046)** | **Capacitive (CST816S)** |
| Touch Interface | **SPI** | **I2C** |
| USB Connector | Micro USB | Micro USB |
| CPU | ESP32-WROOM-32 | ESP32-WROOM-32 |
| Flash | 4MB | 4MB |
| PSRAM | None | None |

## Key Differences

### 1. Display Driver
- **ESP32-2432S028R**: Uses ILI9341 driver
- **JC2432W328C**: Uses ST7789 driver with BGR color order and inversion off

### 2. Touch Controller
- **ESP32-2432S028R**: XPT2046 resistive touch over SPI
- **JC2432W328C**: CST816S capacitive touch over I2C

### 3. Backlight Pin
- **ESP32-2432S028R**: GPIO 21
- **JC2432W328C**: GPIO 27

---

## Configuration for ESP32-2432S028R (Original CYD)

### platformio.ini
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

monitor_speed = 115200

lib_deps =
    bodmer/TFT_eSPI@^2.5.43
    paulstoffregen/XPT2046_Touchscreen@^1.4

build_flags = 
    -DUSER_SETUP_LOADED=1
    -DILI9341_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=12
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=-1
    -DTFT_BL=21
    -DTOUCH_CS=33
    -DLOAD_GLCD=1
    -DLOAD_FONT2=1
    -DLOAD_FONT4=1
    -DLOAD_FONT6=1
    -DLOAD_FONT7=1
    -DLOAD_FONT8=1
    -DLOAD_GFXFF=1
    -DSMOOTH_FONT=1
    -DSPI_FREQUENCY=40000000
    -DSPI_READ_FREQUENCY=20000000
    -DSPI_TOUCH_FREQUENCY=2500000

board_build.partitions = default.csv
board_build.filesystem = littlefs
```

### Touch Code (XPT2046 - SPI Resistive)
```cpp
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// Touch pins - SEPARATE SPI bus from display!
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_SCLK 25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39

// Backlight
#define TFT_BACKLIGHT 21

// Touch calibration values (adjust for your specific board)
#define TOUCH_MIN_X 300
#define TOUCH_MAX_X 3900
#define TOUCH_MIN_Y 300
#define TOUCH_MAX_Y 3900

// Create second SPI bus for touch (HSPI)
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void setup() {
    // Initialize backlight
    pinMode(TFT_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_BACKLIGHT, HIGH);
    
    // Initialize touch on separate SPI bus
    touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);  // Match display rotation
}

bool readTouch(int &x, int &y) {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        
        // Map raw values to screen coordinates
        x = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, 320);
        y = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, 240);
        
        // Clamp to screen bounds
        x = constrain(x, 0, 319);
        y = constrain(y, 0, 239);
        
        return true;
    }
    return false;
}
```

---

## Configuration for JC2432W328C (Capacitive CYD)

### platformio.ini
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

monitor_speed = 115200

lib_deps =
    bodmer/TFT_eSPI@^2.5.43

build_flags = 
    -DUSER_SETUP_LOADED=1
    -DST7789_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_INVERSION_OFF=1
    -DTFT_RGB_ORDER=TFT_BGR
    -DTFT_MISO=12
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=-1
    -DTFT_BL=27
    -DTOUCH_CS=33
    -DLOAD_GLCD=1
    -DLOAD_FONT2=1
    -DLOAD_FONT4=1
    -DLOAD_FONT6=1
    -DLOAD_FONT7=1
    -DLOAD_FONT8=1
    -DLOAD_GFXFF=1
    -DSMOOTH_FONT=1
    -DSPI_FREQUENCY=40000000
    -DSPI_READ_FREQUENCY=20000000

board_build.partitions = default.csv
board_build.filesystem = littlefs
```

### Touch Code (CST816S - I2C Capacitive)
```cpp
#include <Wire.h>

// Touch pins (I2C) - IMPORTANT: Non-standard pins!
#define TOUCH_SDA 33
#define TOUCH_SCL 32
#define TOUCH_RST 25
#define TOUCH_INT 36  // Not used - polling instead

// Backlight
#define TFT_BACKLIGHT 27

// CST816S I2C address
#define CST816S_ADDR 0x15

void setup() {
    // Initialize backlight
    pinMode(TFT_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_BACKLIGHT, HIGH);
    
    // Reset touch controller
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(20);
    digitalWrite(TOUCH_RST, HIGH);
    delay(100);
    
    // Initialize I2C on non-standard pins
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
}

bool readTouch(int &screenX, int &screenY) {
    Wire.beginTransmission(CST816S_ADDR);
    Wire.write(0x02);  // Finger count register
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    
    Wire.requestFrom(CST816S_ADDR, 5);
    if (Wire.available() >= 5) {
        uint8_t fingers = Wire.read();  // 0x02
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
}
```

---

## Pin Mapping Comparison

### Display Pins (Same for both boards)
| Function | GPIO |
|----------|------|
| MISO | 12 |
| MOSI | 13 |
| SCLK | 14 |
| CS | 15 |
| DC | 2 |
| RST | -1 (not connected) |

### Touch Pins
The ESP32-2432S028R uses a **separate SPI bus** for touch (not shared with display).

| Function | ESP32-2432S028R (XPT2046) | JC2432W328C (CST816S) |
|----------|---------------------------|------------------------|
| Interface | SPI (HSPI) | I2C |
| CS / SDA | 33 | 33 |
| SCLK / SCL | 25 | 32 |
| MOSI | 32 | - |
| MISO | 39 | - |
| IRQ / INT | 36 | 36 |
| RST | - | 25 |

### Other Pins
| Function | ESP32-2432S028R | JC2432W328C |
|----------|-----------------|-------------|
| Backlight | 21 | 27 |
| RGB LED R | 22 (common anode) | 4 |
| RGB LED G | 16 (common anode) | 16 |
| RGB LED B | 17 (common anode) | 17 |
| Speaker Enable | 4 | - |
| Speaker DAC | 26 | 26 |
| SD Card CS | 5 | - |
| SD Card MOSI | 23 | - |
| SD Card MISO | 19 | - |
| SD Card SCLK | 18 | - |
| Battery ADC | 34 | - |

---

## Creating a Multi-Board Compatible App

To support both boards with a single codebase, use build-time configuration:

### platformio.ini (Multi-environment)
```ini
[env]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    bodmer/TFT_eSPI@^2.5.43

board_build.partitions = default.csv
board_build.filesystem = littlefs

; Common TFT_eSPI flags
build_flags = 
    -DUSER_SETUP_LOADED=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=12
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=-1
    -DLOAD_GLCD=1
    -DLOAD_FONT2=1
    -DLOAD_FONT4=1
    -DLOAD_FONT6=1
    -DLOAD_FONT7=1
    -DLOAD_FONT8=1
    -DLOAD_GFXFF=1
    -DSMOOTH_FONT=1
    -DSPI_FREQUENCY=40000000
    -DSPI_READ_FREQUENCY=20000000

; ===== ESP32-2432S028R (Original CYD - Resistive Touch) =====
[env:cyd_resistive]
lib_deps = 
    ${env.lib_deps}
    paulstoffregen/XPT2046_Touchscreen@^1.4
build_flags = 
    ${env.build_flags}
    -DBOARD_CYD_RESISTIVE=1
    -DILI9341_DRIVER=1
    -DTFT_BL=21
    -DSPI_TOUCH_FREQUENCY=2500000

; ===== JC2432W328C (Capacitive Touch) =====
[env:cyd_capacitive]
build_flags = 
    ${env.build_flags}
    -DBOARD_CYD_CAPACITIVE=1
    -DST7789_DRIVER=1
    -DTFT_INVERSION_OFF=1
    -DTFT_RGB_ORDER=TFT_BGR
    -DTFT_BL=27
```

### Conditional Code in main.cpp
```cpp
// Board-specific configuration
#if defined(BOARD_CYD_RESISTIVE)
    // ESP32-2432S028R with XPT2046
    #include <XPT2046_Touchscreen.h>
    #define TOUCH_CS 33
    #define TOUCH_IRQ 36
    #define TFT_BACKLIGHT 21
    XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
    
#elif defined(BOARD_CYD_CAPACITIVE)
    // JC2432W328C with CST816S
    #include <Wire.h>
    #define TOUCH_SDA 33
    #define TOUCH_SCL 32
    #define TOUCH_RST 25
    #define TFT_BACKLIGHT 27
    #define CST816S_ADDR 0x15
#endif

void initTouch() {
#if defined(BOARD_CYD_RESISTIVE)
    ts.begin();
    ts.setRotation(1);
#elif defined(BOARD_CYD_CAPACITIVE)
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(20);
    digitalWrite(TOUCH_RST, HIGH);
    delay(100);
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
#endif
}

bool readTouch(int &x, int &y) {
#if defined(BOARD_CYD_RESISTIVE)
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        x = map(p.x, 300, 3900, 0, 320);
        y = map(p.y, 300, 3900, 0, 240);
        return true;
    }
    return false;
    
#elif defined(BOARD_CYD_CAPACITIVE)
    Wire.beginTransmission(CST816S_ADDR);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0) return false;
    
    Wire.requestFrom(CST816S_ADDR, 5);
    if (Wire.available() >= 5) {
        uint8_t fingers = Wire.read();
        uint8_t xh = Wire.read();
        uint8_t xl = Wire.read();
        uint8_t yh = Wire.read();
        uint8_t yl = Wire.read();
        
        if (fingers > 0) {
            uint16_t rawX = ((xh & 0x0F) << 8) | xl;
            uint16_t rawY = ((yh & 0x0F) << 8) | yl;
            x = rawY;
            y = 240 - rawX;
            return true;
        }
    }
    return false;
#endif
}
```

---

## Troubleshooting

### Display Issues
| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| Display shows garbage/noise | Wrong driver | Use ST7789 for JC2432W328C, ILI9341 for original |
| Colors are wrong (blue/red swapped) | Wrong color order | Add `-DTFT_RGB_ORDER=TFT_BGR` for JC2432W328C |
| Display is inverted/negative | Inversion setting | Add `-DTFT_INVERSION_OFF=1` for JC2432W328C |
| Backlight not working | Wrong backlight pin | GPIO 21 for original, GPIO 27 for JC2432W328C |

### Touch Issues
| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| Touch not detected at all | Wrong interface type | XPT2046 (SPI) vs CST816S (I2C) |
| I2C device not found | Wrong I2C pins | JC2432W328C uses SDA=33, SCL=32 (non-standard!) |
| Touch coordinates wrong | Rotation mismatch | Adjust coordinate mapping for screen rotation |
| CST816S library not working | Library issue | Use direct I2C register reads instead |

### I2C Debugging for JC2432W328C
If touch isn't working, scan for I2C devices:
```cpp
Wire.begin(33, 32);  // SDA=33, SCL=32
for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
        Serial.printf("Device found at 0x%02X\n", addr);
    }
}
```
CST816S should appear at address **0x15**.

---

## References

- [ESP32 SmartDisplay Library](https://github.com/rzeldent/esp32-smartdisplay) - Supports many CYD variants
- [Sunton Board Definitions](https://github.com/rzeldent/platformio-espressif32-sunton) - PlatformIO board configs
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI) - Display driver
- [CST816S Datasheet](https://github.com/fbiego/CST816S) - Touch controller reference
