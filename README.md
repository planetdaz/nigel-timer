# Nigel's Potty Timer

A touchscreen timer application for ESP32 "Cheap Yellow Display" (CYD) boards to track our dog Nigel's potty breaks throughout the night.

## Purpose

Nigel wakes us up multiple times at night to go potty. This device sits by the door (visible from bed) and helps us track:
- How long since his last potty break
- Whether it's time to take him out again (via color-coded display)
- Real-time clock display (WiFi + NTP sync)
- A log of all potty break timestamps

## Supported Hardware

This project supports **two CYD board variants** with automatic configuration via PlatformIO environments:

| Board | Display | Touch | Build Command |
|-------|---------|-------|---------------|
| ESP32-2432S028R | ILI9341 | XPT2046 (Resistive/SPI) | `pio run -e cyd_resistive` |
| JC2432W328C | ST7789 | CST816S (Capacitive/I2C) | `pio run -e cyd_capacitive` |

👉 **See [CYD_BOARD_COMPARISON.md](CYD_BOARD_COMPARISON.md) for detailed hardware differences and pin configurations.**

### Common Specifications
- **Display Size:** 2.8" 320×240 TFT LCD
- **CPU:** ESP32-WROOM-32 (Xtensa dual-core @ 240MHz)
- **Memory:** 4MB Flash, 520KB SRAM
- **Interface:** Micro USB

## How It Works

### Features
- **Large Timer Display:** Easy-to-read countdown visible from across the room
- **Real-Time Clock:** Shows current time via WiFi/NTP sync (lower left corner)
- **Color-Coded Background:** Instantly see if it's too soon or time to go out
- **Touch to Reset:** Single tap logs the potty break and resets the timer
- **Log History:** View and clear past potty break timestamps
- **Persistent Storage:** Data survives power cycles (stored in flash)

### On Boot
1. Connects to WiFi and syncs time via NTP
2. Restores timer from last saved state (if available)
3. Display shows timer with real-time clock in lower left
4. Background color indicates time since last break

### Operation
1. **First touch:** Timer starts counting up from 0:00:00
2. **Subsequent touches on timer area:** 
   - Logs entry with timestamp and duration
   - Timer resets to 0:00:00 and immediately restarts
3. **Logs button (bottom right):** View history of potty breaks
4. **Clear button (in logs view):** Delete all log entries

### Color Coding (Visible from Bed)

The background color indicates time since last potty break:

- 🔴 **Red:** 0-3.5 hours (too soon to go again)
- 🟡 **Yellow:** 3.5-4 hours (borderline)
- � **Blue:** 4+ hours (definitely time to go) + chime plays!

*Thresholds are configurable in code.*

### Timer Display

- **Format:** hh:mm:ss (hours:minutes:seconds)
- **Clock:** Real-time display in lower left corner (synced via NTP)
- **Updates:** Every second
- **Touch debounce:** 1 second delay prevents accidental double-touches

### Data Logging

- **Storage:** LittleFS (ESP32 flash memory)
- **File:** `/logs.txt`
- **Format:** Timestamped entries with duration
  ```
  [12:34:56] Duration: 03:45:12
  [16:20:08] Duration: 04:01:23
  ```
- **Timer State:** Saved to preferences, survives power cycles

## Configuration

### WiFi (required for clock)

Edit these constants in `src/main.cpp`:
```cpp
const char* WIFI_SSID = "YourNetwork";
const char* WIFI_PASSWORD = "YourPassword";
const long GMT_OFFSET_SEC = -6 * 3600;  // Your timezone offset
```

### Thresholds (constants in code)

```cpp
const int THRESHOLD_YELLOW = 12600;  // 3.5 hours (seconds)
const int THRESHOLD_BLUE = 14400;    // 4 hours (seconds)
```

### Touch Debouncing

Configurable debounce delay to prevent accidental double-touches (default: 500ms).

### Audio

A chime plays when the timer reaches the blue threshold (4+ hours). Uses ESP8266Audio library with ESP32 internal DAC on GPIO26.

## Development

### Platform

- **Framework:** PlatformIO with multi-environment support
- **Build System:** Arduino framework for ESP32
- **IDE:** VS Code with PlatformIO extension

### Dependencies

- `bodmer/TFT_eSPI` - Display driver library
- `paulstoffregen/XPT2046_Touchscreen` - Touch controller (resistive boards only)
- `earlephilhower/ESP8266Audio` - WAV audio playback via internal DAC
- Built-in ESP32 libraries: `LittleFS.h`, `Preferences.h`, `WiFi.h`

### Building

**For resistive touch board (ESP32-2432S028R):**
```bash
pio run -e cyd_resistive --target upload
```

**For capacitive touch board (JC2432W328C):**
```bash
pio run -e cyd_capacitive --target upload
```

### Setup

1. Install PlatformIO in VS Code
2. Clone this repository
3. Update WiFi credentials in `src/main.cpp`
4. Open folder in VS Code
5. Build and upload for your board variant (see commands above)

## Future Enhancements

- ⚙️ On-screen settings to configure thresholds
- 📊 Statistics view (average duration, frequency patterns)
- 🌙 Night mode (dimmed display after certain hours)
- 🔊 Volume control for audio alerts

## Version

**v2.0** - Multi-board support, WiFi/NTP clock, improved UI with logs screen

## License

Personal project for household use.

## Resources

- [Board Comparison Guide](CYD_BOARD_COMPARISON.md) - Detailed hardware differences
- [ESP32-2432S028R Documentation](https://www.lcdwiki.com/2.8inch_ESP32-32E_Display) - LCD Wiki
- [JC2432W328C Board Definition](https://github.com/rzeldent/platformio-espressif32-sunton) - PlatformIO configs

---

*Built with ❤️ for Nigel the dog* 🐕
