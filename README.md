# Y'all-ARM

A desktop IoT device that monitors [Ryan Hall Y'all's](https://ryanhallyall.com) Weather Intensity Score (WIS) and lights up a 3D-printed LED display when he goes live. The progress bar shows the current WIS score as a percentage of today's stream-trigger threshold in real time.

---

## What It Does

- **Polls `ryanhallyall.com/rhy/wis.json` every 60 seconds** — no YouTube API key required
- **Progress bar** (10 LEDs) shows how close the WIS score is to today's stream threshold
  - Blue → Teal → Magenta gradient as score climbs
  - Full strobe when score hits 100% of threshold
- **Goes live alert** — when Ryan's stream mode flips from `off`, the ON AIR logo pulses, the bar runs a climb animation bottom-to-top, and an MP3 audio clip plays
- **Web dashboard** at the device's local IP — shows live WIS data and lets you test the LEDs manually
- **WiFi setup via captive portal** — no credentials hardcoded, configure from your phone on first boot

---

## Hardware

| Component | Part | Notes |
|-----------|------|-------|
| Microcontroller | ESP32-S3 DevKit | Any ESP32-S3 DevKitC-1 variant |
| LEDs | WS2812B strip (16 LEDs) | NeoPixel-compatible |
| Audio DAC/Amp | MAX98357A | I2S, drives a small speaker directly |
| Button (optional) | Tactile switch | Dismisses the alert |
| GPS module (optional) | NEO-6M / NEO-M8N / L76 | Only needed for GPS-based location (Method 2); ~$8–15 over UART |

### Wiring

| Signal | GPIO |
|--------|------|
| LED Data | 18 |
| I2S BCLK | 14 |
| I2S LRC | 15 |
| I2S DIN | 16 |
| Dismiss Button | 0 (boot button) |

Optional GPS module (only if using Method 2 below):

| Signal | GPIO | Notes |
|--------|------|-------|
| GPS TX → ESP32 RX | 17 | UART RX |
| GPS RX → ESP32 TX | 8 | UART TX |
| GPS VCC | 3.3V | — |
| GPS GND | GND | — |

### LED Layout

```
LEDs 0–5   → ON AIR logo
LEDs 6–15  → WIS progress bar (6 = bottom, 15 = top)
```

---

## LED Behavior

| State | ON AIR Logo | Progress Bar |
|-------|-------------|--------------|
| Idle | Slow teal breath | Live WIS % fill (Blue/Teal/Magenta) |
| Alert (just went live) | Pulsing teal | Climb animation: Blue → Teal → Magenta |
| Live | Solid teal | Live WIS % fill; top 2 LEDs strobe at 100% |
| Manual override | Solid teal | Fixed % you set from the dashboard |

**Bar color by WIS %:**
- 1–40% → Blue
- 41–70% → Teal
- 71–100% → Magenta (+ strobe at 100%)

---

## Location (Optional)

The device can optionally determine its own location via one of two methods. Both are off by default. Enable either, both, or neither in `include/config.h`. When both are enabled, GPS is preferred with IP-based as fallback.

| Method | Hardware | Accuracy | Notes |
|--------|----------|----------|-------|
| IP-based geolocation | None (uses existing WiFi) | City-level (1–50 km) | Easiest option; zero BOM impact |
| GPS module | NEO-6M / NEO-M8N / L76 over UART | 2–5 m outdoors | Requires clear sky view; typically no fix indoors |

**Method 1 — IP-based geolocation**

Uses the existing WiFi connection to query a free service (e.g. `http://ip-api.com/json/`). No API key required on the free tier (~45 req/min limit). Parsed with ArduinoJson, cached in NVS/LittleFS, refreshed daily.

**Method 2 — Onboard GPS module**

Adds a NEO-6M / NEO-M8N / L76-style GPS module on UART (see wiring above). Uses [`mikalhart/TinyGPSPlus`](https://github.com/mikalhart/TinyGPSPlus) — add to `lib_deps` when enabled. Typical cold-start time to first fix: 30–60 seconds. Essentially useless indoors without a window view — noteworthy for a desk device. Best for portable/travel use or when precise location is needed.

Resolved location is exposed on the `/status` JSON endpoint and shown on the dashboard.

```cpp
// Enable one, both, or neither
#define LOCATION_METHOD_IP_GEOLOCATION  1
#define LOCATION_METHOD_GPS             0

#define IP_GEOLOCATION_URL              "http://ip-api.com/json/?fields=lat,lon,city,regionName,country,timezone"

#define GPS_UART_RX_PIN                 17
#define GPS_UART_TX_PIN                 8
#define GPS_UART_BAUD                   9600
```

---

## Software Setup

### Requirements

- [PlatformIO](https://platformio.org/) CLI
- Python 3 (for PlatformIO)

### Build & Flash

```bash
# Clone the repo
git clone https://github.com/ericbram/yallarm.git
cd yallarm

# Flash the firmware
pio run --target upload

# Upload the audio file (after adding yall_live.mp3 to data/)
pio run --target uploadfs
```

### Audio File

Place your alert MP3 at `data/yall_live.mp3` before running `uploadfs`.

Requirements:
- Format: MP3
- Sample rate: 44.1kHz or 22.05kHz
- Bitrate: 128kbps or lower
- Channels: mono or stereo

---

## Configuration

**All settings live in `include/config.h`** — it's the only file you need to touch to customize the device.

```cpp
// Change the WIS poll URL
#define WIS_API_URL          "https://ryanhallyall.com/rhy/wis.json"

// Change how often it polls (ms)
#define WIS_POLL_INTERVAL_MS (300 * 1000)

// Change LED pins, brightness, animation speed, colors...
// Change audio pins and volume...
// Change button pin...
```

WiFi credentials are **not** stored in code — they're configured at runtime through the captive portal.

---

## First Boot (WiFi Setup)

1. Power on the device
2. It will broadcast a WiFi network called **`YallARM-Setup`**
3. Connect from your phone — a captive portal will open automatically
4. Enter your home WiFi SSID and password
5. The device saves credentials to flash and reboots onto your network

To reconfigure WiFi later, hold the dismiss button on boot (or use the WiFiManager reset flow).

---

## Web Dashboard

Once on your network, the device hosts a dashboard at its local IP address (printed to serial on boot).

**`http://<device-ip>/`** — Status page showing:
- Current WIS score and today's stream threshold
- WIS % (score as % of threshold)
- 30-minute forecast score
- Live / Offline status
- Manual override slider for testing LEDs

**`http://<device-ip>/status`** — JSON snapshot for scripting/integration:
```json
{
  "wis_score": 14.49,
  "threshold": 147.16,
  "wis_pct": 9,
  "score_30m": 13.59,
  "is_live": false,
  "mode": "off",
  "state": "IDLE",
  "location": {
    "lat": 35.2271,
    "lon": -80.8431,
    "city": "Charlotte",
    "source": "ip"
  }
}
```

The `location` object is present only when a location method is enabled. `source` is `"ip"` or `"gps"`.

---

## Project Structure

```
yallarm/
├── include/
│   ├── config.h       # All configuration — start here
│   ├── wis.h          # WIS API types and declarations
│   ├── leds.h         # LED state machine declarations
│   └── audio.h        # Audio playback declarations
├── src/
│   ├── main.cpp       # Setup, loop, web server, state machine
│   ├── wis.cpp        # HTTP polling and WIS % calculation
│   ├── leds.cpp       # FastLED animations and state machine
│   └── audio.cpp      # I2S audio via ESP32-audioI2S
├── data/
│   └── yall_live.mp3  # Alert audio (add your own — not in repo)
├── platformio.ini     # Build config and library dependencies
└── AGENTS.md          # Full implementation spec
```

---

## Libraries Used

| Library | Purpose |
|---------|---------|
| [FastLED](https://github.com/FastLED/FastLED) | WS2812B LED control |
| [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) | MP3 playback via I2S |
| [ArduinoJson](https://arduinojson.org/) | WIS API JSON parsing |
| [WiFiManager](https://github.com/tzapu/WiFiManager) | Captive portal WiFi setup |
| [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus) | GPS NMEA parsing (only if using GPS) |

---

## License

MIT — see [LICENSE](LICENSE)
