# Y'all-ARM — AI Model Context

Paste this document at the start of a conversation to give an AI model full context
about the Y'all-ARM project without requiring it to read all source files.

---

## What This Is

**Y'all-ARM** is an ESP32-S3 desktop IoT device that monitors
[Ryan Hall Y'all's](https://ryanhallyall.com) Weather Intensity Score (WIS) API.
It lights up a 3D-printed LED display and plays an audio alert when he goes live,
and shows a real-time progress bar of the current WIS score as a percentage of
today's stream-trigger threshold.

---

## Hardware

| Component       | Part                   | Notes                                        |
|-----------------|------------------------|----------------------------------------------|
| MCU             | ESP32-S3 DevKitC-1     | Any N8R2 or N16R8 variant                    |
| LEDs            | WS2812B strip, 16 LEDs | NeoPixel-compatible, 5V                      |
| Audio amp       | MAX98357A              | I2S DAC/amp, drives a small speaker directly |
| Dismiss button  | Tactile switch         | Optional; GPIO 0 / BOOT button               |

**Pin map:**

| Signal       | GPIO |
|--------------|------|
| LED data     | 18   |
| I2S BCLK     | 14   |
| I2S LRC      | 15   |
| I2S DIN      | 16   |
| Dismiss btn  | 0    |

**LED layout:**
- LEDs 0–5 → ON AIR logo
- LEDs 6–15 → WIS progress bar (6 = bottom, 15 = top)

---

## Firmware Architecture

**Language / toolchain:** C++20, Arduino framework, PlatformIO

**Key source files:**

| File                  | Purpose                                               |
|-----------------------|-------------------------------------------------------|
| `src/main.cpp`        | Setup, loop, web server, state machine                |
| `src/wis.cpp`         | HTTP polling and WIS % calculation                    |
| `src/leds.cpp`        | FastLED animations and LED state machine              |
| `src/audio.cpp`       | I2S audio playback via ESP32-audioI2S                 |
| `include/config.h`    | All tunable constants — the only file users edit      |
| `include/wis_calc.h`  | Pure WIS logic (no hardware deps; used by unit tests) |
| `include/led_calc.h`  | Pure LED calc logic (no hardware deps; unit tests)    |

**Libraries:**

| Library              | Purpose                            |
|----------------------|------------------------------------|
| FastLED              | WS2812B LED control                |
| ESP32-audioI2S       | MP3 playback over I2S              |
| ArduinoJson          | WIS API JSON parsing               |
| WiFiManager          | Captive portal WiFi setup          |

---

## State Machine

```
STATE_IDLE    — not live; logo breathes dim teal; bar shows live WIS %
STATE_ALERT   — just went live; logo pulses; bar runs climb animation
STATE_LIVE    — live, alert played; logo solid teal; bar shows live WIS %
STATE_OVERRIDE — manual override from web dashboard; ignores live data
```

Transitions:
- `IDLE + is_live → ALERT`
- `ALERT → LIVE` (auto, after climb animation)
- `LIVE + !is_live → IDLE`
- `OVERRIDE` stays until `POST /reset`

---

## WIS API

```
GET https://ryanhallyall.com/rhy/wis.json   (no auth)
```

Fields used:
- `wis.weather_intensity_score` — current WIS score
- `wis.weather_intensity_score_30m_from_now` — 30m forecast
- `wis.todays_stream_info.weather_intensity_score_threshold` — today's stream trigger
- `wis.todays_stream_info.mode` — `"off"` = not live; anything else = live

`wis_pct = clamp(round(score / threshold * 100), 1, 100)`. If `threshold < 10.0`
(WIS_THRESHOLD_FLOOR), return 1 to avoid false "maxed out" display.

Poll interval: 5 minutes (configurable in `config.h`).

---

## LED Behavior

**Progress bar colors (by wis_pct):**
- 1–40% → Blue
- 41–70% → Teal
- 71–100% → Magenta (+ strobe on top 2 LEDs at exactly 100%)

**Bar fill:** `round(wis_pct / 10.0)` LEDs lit out of 10 (so 50% = 5 LEDs).

---

## Web Dashboard

Device hosts a minimal HTTP server at its local IP.

| Endpoint    | Method | Description                                  |
|-------------|--------|----------------------------------------------|
| `/`         | GET    | HTML dashboard with WIS data and override UI |
| `/status`   | GET    | JSON snapshot of current state               |
| `/override` | POST   | `level=N` (1–100) — activates STATE_OVERRIDE |
| `/reset`    | POST   | Clears override, returns to live polling     |

`/status` response:
```json
{
  "wis_score": 14.49, "threshold": 147.16, "wis_pct": 9,
  "score_30m": 13.59, "is_live": false, "mode": "off", "state": "IDLE"
}
```

---

## Configuration (`include/config.h`)

All user-tunable settings live here. Key defines:

```cpp
// WiFi — optional compile-time creds (skip captive portal during dev)
// #define WIFI_SSID     "YourNetwork"
// #define WIFI_PASSWORD "YourPassword"
#define WIFI_AP_NAME        "YallARM-Setup"
#define WIFI_AP_TIMEOUT_S   180

// WIS
#define WIS_API_URL          "https://ryanhallyall.com/rhy/wis.json"
#define WIS_POLL_INTERVAL_MS (300 * 1000)
#define WIS_THRESHOLD_FLOOR  10.0f

// LED
#define LED_DATA_PIN    18
#define NUM_LEDS        16
#define LED_BAR_COUNT   10
#define LED_BRIGHTNESS  128

// Audio (MAX98357A, 0–21)
#define I2S_BCLK_PIN    14
#define I2S_LRC_PIN     15
#define I2S_DOUT_PIN    16
#define AUDIO_VOLUME    15
```

---

## Testing

Tests live in `test/` and run on the host (no hardware required):

```bash
pio test -e native        # run all unit tests
```

Three test suites:
- `test_wis` — WIS % calculation and live detection logic
- `test_leds` — bar LED count, color tier, strobe logic
- `test_config` — config constants are internally self-consistent and in valid ranges

The pure logic is isolated into `include/wis_calc.h` and `include/led_calc.h`
(header-only, no Arduino deps) so it can compile and run natively.

---

## Build & Flash

```bash
pio run -e esp32-s3-devkitc-1      # compile firmware
pio run --target upload             # compile + flash over USB
pio run --target uploadfs           # flash LittleFS (audio file)
```

The `data/` directory holds `yall_live.mp3` (user-supplied, not in repo).
Format: MP3, ≤128kbps, 44.1kHz or 22.05kHz.

---

## CI/CD

- **`ci.yml`** — runs on push/PR to main; runs unit tests + firmware build
- **`release.yml`** — runs on GitHub release publish; builds firmware and uploads
  `firmware.bin`, `partitions.bin`, `bootloader.bin` as release assets

---

## What Is NOT Implemented

- **Location / geolocation** — stripped; see `docs/YALLCALL.md`
- **YallCall geofence alerts** — future feature spec in `docs/YALLCALL.md`; depends
  on Ryan Hall publishing a YallCall API with geographic alert areas

---

## Key Constraints & Decisions

- `wis_pct` is clamped to 1–100 (never 0 — the bar always shows at least 1 LED
  unless `threshold < WIS_THRESHOLD_FLOOR`, which also returns 1)
- `mode != "off"` is the live detection rule — "off" is the only known non-live value;
  everything else is treated as live (fail open)
- WiFiManager handles WiFi credentials at runtime via captive portal. Optional
  `WIFI_SSID` / `WIFI_PASSWORD` defines in `config.h` skip the portal (dev convenience)
- ESP32-audioI2S is pinned to v2.0.6 — later releases introduced a `<span>` header
  dependency that breaks the PlatformIO build
- Flash layout uses the full 8MB via a custom partition table (`default_8MB.csv`)
- No OTA update support yet
