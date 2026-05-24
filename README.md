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
| Microcontroller | ESP32-S3-DevKitC-1-N8R8 | Any ESP32-S3 DevKitC-1 variant works; this build is wired for the N8R8 |
| LEDs | WS2812B strip (16 LEDs) | NeoPixel-compatible |
| Audio DAC/Amp | MAX98357A | I2S, drives a small speaker directly |
| Button (optional) | Tactile switch | Dismisses the alert |

### Wiring

Power everything from the **5V** pin on the DevKit (USB-C provides 5V), not the 3V3 pin — the 3.3V regulator can't supply enough current for the LEDs and the amp running together. Tie all grounds together (LED strip, amp, ESP32, PSU).

| Signal | GPIO | Connects to |
|--------|------|-------------|
| LED Data | 18 | WS2812B `DIN` (via 330–470Ω resistor) |
| LED 5V | 5V | WS2812B `5V` |
| LED GND | GND | WS2812B `GND` |
| I2S BCLK | 15 | MAX98357A `BCLK` |
| I2S LRC | 16 | MAX98357A `LRC` |
| I2S DIN | 17 | MAX98357A `DIN` |
| Amp 5V | 5V | MAX98357A `VIN` (5V gives more headroom than 3V3) |
| Amp GND | GND | MAX98357A `GND` |
| Dismiss Button | 4 | Button → GND (firmware uses INPUT_PULLUP — no external resistor) |

**Notes:**
- Add a 330–470Ω series resistor between GPIO 18 and the LED strip's `DIN`.
- A 1000µF capacitor across the strip's 5V/GND smooths inrush current.
- The picked GPIOs (4, 15, 16, 17, 18) avoid the ESP32-S3 strapping pins (0, 3, 45, 46), so the board boots cleanly every power cycle.
- The DevKitC-1 has two USB-C ports labeled `UART` and `USB`. Use the `UART` port for flashing and serial monitoring.

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

#### Sample Clips

The [`samples/`](samples/) directory holds example MP3s you can drop into `data/` to try things out. For example, to use the included `weather.mp3` as your alert sound:

```bash
cp samples/weather.mp3 data/yall_live.mp3
pio run --target uploadfs
```

#### Audio Self-Test

The dashboard has an **Audio Test** section that streams any MP3 URL through the speaker so you can verify the I2S amp and wiring without flashing a filesystem. Paste a URL, click **Play Audio**, and you should hear it within a second or two. Leave the URL field blank to play the file currently at `/yall_live.mp3` in LittleFS.

The same can be triggered headlessly:

```bash
# Stream a remote MP3
curl -X POST http://<device-ip>/test-audio -d "url=https://example.com/clip.mp3"

# Play the local alert file
curl -X POST http://<device-ip>/test-audio

# Stop playback
curl -X POST http://<device-ip>/test-audio-stop
```

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

### WiFi Credentials

Two ways to get the device on WiFi:

1. **Captive portal** (default, zero setup) — power it on, join the `YallARM-Setup` network, pick your WiFi in the portal. Credentials are persisted to flash; subsequent boots reconnect automatically.
2. **Compile-time via `include/secrets.h`** (skips the portal on every flash) — recommended for active development. The file is gitignored so credentials never end up in source control.

To use option 2:

```bash
cp include/secrets.h.example include/secrets.h
# edit include/secrets.h and set WIFI_SSID / WIFI_PASSWORD
pio run --target upload
```

If `include/secrets.h` is missing or the credentials don't connect within ~15 seconds, the firmware falls back to the captive portal automatically.

---

## First Boot (WiFi Setup, captive portal)

1. Power on the device
2. It will broadcast a WiFi network called **`YallARM-Setup`**
3. Connect from your phone — a captive portal will open automatically
4. Enter your home WiFi SSID and password
5. The device saves credentials to flash and reboots onto your network

To reconfigure WiFi later, use the WiFiManager reset flow from the dashboard, or reflash with `include/secrets.h` updated (or removed) and erase flash.

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
  "state": "IDLE"
}
```

---

## Project Structure

```
yallarm/
├── include/
│   ├── config.h            # All configuration — start here
│   ├── secrets.h.example   # Copy → secrets.h to bake WiFi creds in (gitignored)
│   ├── wis.h               # WIS API types and declarations
│   ├── leds.h              # LED state machine declarations
│   └── app_audio.h         # Audio playback declarations
├── src/
│   ├── main.cpp            # Setup, loop, web server, state machine
│   ├── wis.cpp             # HTTP polling and WIS % calculation
│   ├── leds.cpp            # FastLED animations and state machine
│   └── app_audio.cpp       # I2S audio via ESP32-audioI2S
├── data/
│   └── yall_live.mp3  # Alert audio (add your own — not in repo)
├── samples/
│   └── weather.mp3    # Example MP3 — copy into data/ to use
├── platformio.ini     # Build config and library dependencies
└── docs/
    ├── AGENTS.md      # Full implementation spec
    ├── INSTALL.md     # Build and flash guide
    ├── YALLCALL.md    # Future feature spec (YallCall geofence alerts)
    └── MODEL_PROMPT.md # AI model context document
```

---

## Libraries Used

| Library | Purpose |
|---------|---------|
| [FastLED](https://github.com/FastLED/FastLED) | WS2812B LED control |
| [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) | MP3 playback via I2S |
| [ArduinoJson](https://arduinojson.org/) | WIS API JSON parsing |
| [WiFiManager](https://github.com/tzapu/WiFiManager) | Captive portal WiFi setup |

---

## License

MIT — see [LICENSE](LICENSE)
