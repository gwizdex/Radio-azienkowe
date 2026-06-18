# Copilot Instructions – Radio-łazienkowe

## Project Overview

This is an **Arduino/C++ project** for a smart bathroom radio running on an **ESP32-S3 WROOM-1 N16R8** microcontroller. The device plays internet radio streams, controlled automatically by ambient light (BH1750 sensor) or manually via a built-in web UI.

- **Language**: C++ (Arduino framework)
- **Target platform**: ESP32-S3 (Espressif)
- **Single source file**: `radio.ino` (~1700 lines)
- **No automated build/test pipeline** — code is compiled and flashed to hardware using the Arduino IDE or `arduino-cli`

---

## Repository Layout

```
Radio-łazienkowe/
├── radio.ino          # All application logic (single Arduino sketch file)
├── README.md          # Project documentation (Polish + English)
└── .github/
    └── copilot-instructions.md
```

All functionality lives in `radio.ino`. There are no additional source files, headers, or library folders in the repo — dependencies are managed through the Arduino IDE Library Manager.

---

## Hardware

| Component | Role |
|-----------|------|
| ESP32-S3 WROOM-1 N16R8 | Main microcontroller (WiFi, audio) |
| MAX98357A | I2S digital audio amplifier |
| BH1750 | Ambient light sensor (I²C, address `0x23`) |
| DHT22 (optional) | Temperature & humidity sensor |
| 4Ω sauna speaker | Audio output |

**Default GPIO assignments** (configurable at runtime and saved in NVS):

| Signal | Default GPIO |
|--------|-------------|
| I²C SDA | 8 |
| I²C SCL | 9 |
| I2S DOUT | 6 |
| I2S BCLK | 4 |
| I2S LRC | 5 |
| DHT PIN | 10 |

---

## Key Libraries (Arduino Library Manager)

- `WiFiManager` — captive-portal WiFi provisioning
- `ESP32-audioI2S` (`Audio.h`) — audio streaming over I2S
- `PubSubClient` — MQTT client
- `DHT sensor library` — DHT22 temperature/humidity
- `Wire` / `WebServer` / `Preferences` / `ESPmDNS` — ESP32 built-ins

---

## Arduino IDE / arduino-cli Settings

These settings **must** be used when compiling or flashing:

```
Board:             ESP32S3 Dev Module
USB CDC On Boot:   Enabled
CPU Frequency:     240MHz (WiFi)
Flash Mode:        QIO 80MHz
Flash Size:        16MB (128Mb)
Partition Scheme:  16M Flash (3MB APP/9.9MB FATFS)
PSRAM:             OPI PSRAM
Upload Mode:       UART0 / Hardware CDC
Upload Speed:      921600
USB Mode:          Hardware CDC and JTAG
Core Debug Level:  None
```

---

## Application Architecture

The sketch follows the standard Arduino `setup()` / `loop()` pattern:

- **`setup()`** — initialises WiFi (WifiManager AP on first boot, SSID: `Radio_Config`, password: `password123`), I²C, I2S audio, web server routes, NVS preferences, MQTT (if enabled), NTP time sync.
- **`loop()`** — reads the BH1750 sensor, evaluates auto/manual mode and schedule, manages audio playback, handles MQTT keep-alive, reads DHT22.

### Persistence (NVS via `Preferences`)

All user settings are stored in the ESP32's Non-Volatile Storage using the `Preferences` library. Key groups:

| Namespace key prefix | Stored data |
|----------------------|-------------|
| `sda_pin`, `scl_pin`, `i2s_*`, `dht_pin`, `dht_enabled` | GPIO configuration |
| `mqtt_server`, `mqtt_port`, `mqtt_user`, `mqtt_pass`, `mqtt_enabled`, `mqtt_prefix` | MQTT settings |
| `stationCount`, `name_N`, `url_N` | Radio stations (up to 20) |
| `threshold`, `delayOn`, `delayOff` | Light trigger settings |
| `schedStartH/M`, `schedEndH/M`, `schedEnabled` | Playback schedule |
| `volume`, `station`, `mode`, `language` | Current playback state |

### Web UI

An embedded HTML/CSS/JS interface is served from `PROGMEM` strings inside `radio.ino`. The server handles both GET (page) and POST (actions) routes. The service mode is password-protected (password: `jolka`).

### TTS (Text-to-Speech)

Google Translate TTS (`translate.google.com/translate_tts`) is used for voice announcements (IP address on boot, mode changes). The `isSpeaking` volatile flag blocks `loop()` during TTS playback.

### MQTT

When enabled, the device subscribes to `<prefix>/cmd/*` topics and publishes state to `<prefix>/state/*` every 10 seconds.

---

## Build & Validation Notes

- **There is no CI pipeline or automated test suite.** Validation is done by flashing to hardware and observing Serial Monitor output (115200 baud).
- The only source file is `radio.ino`. All changes are made there.
- When adding new NVS keys, always provide a sensible default value in the `getXxx()` call to avoid issues on first boot after an update.
- The `Audio` library is non-blocking but requires `audio.loop()` to be called every iteration of `loop()`. Do not add long `delay()` calls inside `loop()`.
- `isSpeaking` flag is used to pause `loop()` logic during TTS; always respect this flag when adding new loop-level logic.
- Up to `MAX_STATIONS` (20) radio stations can be stored.
- The web UI HTML/CSS/JS is stored in `PROGMEM` raw string literals (`R"rawliteral(...)rawliteral"`). When editing the UI, keep changes within the existing string blocks.
- Comments and variable names in the code are in **Polish**; keep new additions consistent with the existing language style (Polish comments, English identifiers are both acceptable as already mixed).
