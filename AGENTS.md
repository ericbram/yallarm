# AGENTS.md — Y'all-ARM Weather Signal: Agent/Coding Specification

## Project Overview

**Name:** Y'all-ARM
**Hardware:** ESP32-S3 DevKit
**Purpose:** A desktop IoT device that monitors Ryan Hall Y'all's WIS (Weather Intensity Score) API. It activates a 3D-printed LED display and audio alert when he goes live, and continuously shows a progress bar of the current WIS score as a percentage of today's stream-trigger threshold.

---

## 1. Project Structure

```
yallarm/
├── AGENTS.md              # This file
├── YALLCALL.md            # Future feature spec: YallCall geofence alerts
├── src/
│   ├── main.cpp           # Primary application entry point
├── data/
│   └── yall_live.mp3      # Audio alert file (stored in LittleFS)
├── include/
│   ├── config.h           # Pin definitions, constants, endpoint URL
│   ├── leds.h             # LED state machine declarations
│   ├── audio.h            # Audio playback declarations
│   └── wis.h              # WIS API polling declarations
├── platformio.ini         # PlatformIO build configuration
└── README.md
```

---

## 2. Target Hardware & Pin Definitions

| Component         | Type                    | Pin(s)                                                  |
|-------------------|-------------------------|---------------------------------------------------------|
| LED Strip         | WS2812B                 | Data: GPIO 18                                           |
| Audio DAC/Amp     | MAX98357A               | BCLK: 14, LRC: 15, DIN: 16                              |
| Optional Button   | Tactile switch          | GPIO 0 (boot button or spare)                           |

### LED Segment Map

| Segment          | LED Indices | Purpose                                    |
|------------------|-------------|--------------------------------------------|
| ON AIR Logo      | 0 – 5       | Live indicator                             |
| WIS Progress Bar | 6 – 15      | WIS score as % of stream threshold (1–100) |

---

## 3. Software Libraries

Declare all dependencies in `platformio.ini`:

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200

lib_deps =
    bblanchon/ArduinoJson @ ^7.0.0
    fastled/FastLED @ ^3.7.0
    schreibfaul1/ESP32-audioI2S @ ^2.0.0
    tzapu/WiFiManager @ ^2.0.17

board_build.filesystem = littlefs
```

Standard ESP32 libraries used (no `lib_deps` entry needed):
- `WiFi.h`
- `HTTPClient.h`
- `LittleFS.h`
- `WebServer.h`

---

## 4. Configuration (`include/config.h`)

```cpp
// WiFi — credentials handled at runtime by WiFiManager, not hardcoded

// WIS API (no authentication required)
const char* WIS_API_URL         = "https://ryanhallyall.com/rhy/wis.json";
const int   WIS_POLL_INTERVAL_MS = 300 * 1000;  // 5m

// Minimum threshold floor — if threshold < this, skip bar fill to avoid
// false "maxed out" display on low-activity days
const float WIS_THRESHOLD_FLOOR = 10.0f;

// LED
#define LED_DATA_PIN    18
#define NUM_LEDS        16
#define LED_BAR_COUNT   10
#define LED_LOGO_START  0
#define LED_LOGO_END    5
#define LED_BAR_START   6
#define LED_BAR_END     15

// Audio I2S
#define I2S_BCLK_PIN    14
#define I2S_LRC_PIN     15
#define I2S_DIN_PIN     16
#define AUDIO_FILE      "/yall_live.mp3"

// Dismiss button
#define DISMISS_PIN     0

// Colors (GRB order for WS2812B)
#define COLOR_TEAL      CRGB(0, 128, 128)
#define COLOR_TEAL_DIM  CRGB(0, 20, 20)
#define COLOR_BLUE      CRGB(0, 0, 200)
#define COLOR_MAGENTA   CRGB(200, 0, 200)
#define COLOR_OFF       CRGB(0, 0, 0)

```

---

## 5. WIS API Polling (`include/wis.h` / `src/wis.cpp`)

### Endpoint

```
GET https://ryanhallyall.com/rhy/wis.json
(no authentication required)
```

### Response Fields Used

| JSON Path | Type | Meaning |
|-----------|------|---------|
| `wis.weather_intensity_score` | float | Current WIS score |
| `wis.weather_intensity_score_30m_from_now` | float | 30-minute forecast score |
| `wis.todays_stream_info.weather_intensity_score_threshold` | float | Score at which Ryan streams today |
| `wis.todays_stream_info.mode` | string | Stream state — `"off"` = not live; any other value = live |

### Returned Struct

```cpp
struct WisData {
    float   current_score;   // wis.weather_intensity_score
    float   score_30m;       // wis.weather_intensity_score_30m_from_now
    float   threshold;       // wis.todays_stream_info.weather_intensity_score_threshold
    String  mode;            // wis.todays_stream_info.mode
    int     wis_pct;         // computed: see below — range 1–100
    bool    is_live;         // computed: mode != "off"
    bool    valid;           // false if HTTP or parse error
};
```

### WIS Percentage Calculation

The canonical value used everywhere (LEDs, dashboard, state machine) is `wis_pct` — the current score expressed as a percentage of today's threshold, clamped to the range 1–100:

```cpp
// Computed immediately after parsing, stored in WisData.wis_pct
if (wis.threshold < WIS_THRESHOLD_FLOOR) {
    wis.wis_pct = 1;  // threshold too low to be meaningful — show floor
} else {
    float raw = (wis.current_score / wis.threshold) * 100.0f;
    wis.wis_pct = (int)constrain(raw, 1.0f, 100.0f);
}
```

Examples at today's threshold of 147.16:
- score 14.5  → `(14.5 / 147.16) * 100` = **9%**
- score 73.6  → `(73.6 / 147.16) * 100` = **50%**
- score 147.2 → **100%** (clamped) — all LEDs + strobe

### Live Detection

```cpp
wis.is_live = (wis.mode != "off");
```

`"off"` is the only observed non-live value. Any other mode value (e.g., `"live"`, `"active"`) is treated as live. Log the raw `mode` string to Serial on every poll so unknown values are visible during testing.

### HTTP Parsing Notes

- Use `WiFiClientSecure` with the ryanhallyall.com root CA for HTTPS, or fall back to plain HTTP (`WiFiClient`) if TLS causes memory pressure.
- Parse only the top-level `wis` object using `ArduinoJson`. Use a `StaticJsonDocument<512>` — do **not** deserialize `score_history` or `daily_outlook_scores` (they are large arrays not needed here). Use `JsonObject wis_obj = doc["wis"]` to scope the parse.
- On HTTP error or JSON parse failure: return the previous cached `WisData` with `valid = false` and log to Serial. Do **not** reset `is_live` on a failed poll — preserve last known state.

### Poll Rate

Poll every **60 seconds**. The server updates approximately every 20 seconds, so 60s is conservative but safe for an embedded device making TLS calls.

---

## 6. LED State Machine (`include/leds.h` / `src/leds.cpp`)

### States

```cpp
enum LedState {
    STATE_IDLE,      // Not live — logo breathes dim teal; bar shows real WIS %
    STATE_ALERT,     // Just went live — logo pulses; bar runs climb animation once
    STATE_LIVE,      // Live, alert played — logo solid bright teal; bar shows real WIS %
    STATE_OVERRIDE,  // Manual override from web dashboard — ignores live WIS data
};
```

### Behavior per State

| State          | Logo (LEDs 0–5)                          | Progress Bar (LEDs 6–15)                                  |
|----------------|------------------------------------------|-----------------------------------------------------------|
| `STATE_IDLE`   | Slow breathe, dim teal                   | Real-time WIS fill based on `wis_pct` (see gradient below) |
| `STATE_ALERT`  | Pulse bright teal (3 cycles)             | Climb animation: Blue → Teal → Magenta, then settle to WIS fill |
| `STATE_LIVE`   | Solid bright teal                        | Real-time WIS fill based on `wis_pct`; strobe if `wis_pct == 100` |
| `STATE_OVERRIDE` | Solid bright teal                      | Fill based on `override_pct` (1–100); strobe if `override_pct == 100` |

### Bar Fill — LED Count from `wis_pct`

```cpp
// wis_pct is 1–100. Map to 0–10 LEDs.
// Use round() so 50% = 5 LEDs, 95% = 10 LEDs (not 9).
int barLeds = (int)round(wis_pct / 10.0f);
barLeds = constrain(barLeds, 0, LED_BAR_COUNT);
```

### Bar Fill — Color Gradient

Color each lit LED based on the overall `wis_pct`, not its individual position:

| `wis_pct` range | Color              |
|-----------------|--------------------|
| 1 – 40          | `COLOR_BLUE`       |
| 41 – 70         | `COLOR_TEAL`       |
| 71 – 100        | `COLOR_MAGENTA`    |

Unlit LEDs are `COLOR_OFF`.

### Breath Effect (Idle Logo)

Use a sine wave over `millis()` to modulate brightness between 5 and 40 on the teal channel. Non-blocking — computed each `loop()` tick.

### Climb Animation (Alert → Live Transition)

Run once on entry into `STATE_ALERT`. Iterate LEDs 6–15 bottom to top: first pass blue, second pass teal, third pass magenta. 80ms per LED step. After animation completes, set state to `STATE_LIVE` and resume real-time WIS fill.

### Strobe (wis_pct == 100)

When `wis_pct == 100` (score has reached or exceeded today's threshold), all 10 bar LEDs fill magenta and the top 2 LEDs (indices 14–15) strobe. Every 250ms, toggle between `COLOR_MAGENTA` and `COLOR_OFF` on those two LEDs. Non-blocking via `millis()` timer.

Same logic applies in `STATE_OVERRIDE` when `override_pct == 100`.

---

## 7. Audio Playback (`include/audio.h` / `src/audio.cpp`)

- Use `ESP32-audioI2S` library.
- Initialize I2S with `BCLK=14`, `LRC=15`, `DOUT=16`.
- On transition into `STATE_ALERT`: call `audio.connecttoFS(LittleFS, AUDIO_FILE)` to play `yall_live.mp3` once.
- Do **not** loop audio — play once per live event detection.
- On transition from `STATE_LIVE` or `STATE_ALERT` back to `STATE_IDLE`: call `audio.stopSong()`.
- Call `audio.loop()` every iteration of the main `loop()` to service playback.

---

## 8. Web Dashboard

The ESP32 hosts a minimal HTTP server on port 80 using Arduino's `WebServer.h`.

### Endpoints

| Method | Path        | Description |
|--------|-------------|-------------|
| GET    | `/`         | Status page showing live WIS data and controls |
| GET    | `/status`   | JSON snapshot (see example below) |
| POST   | `/override` | Accepts `level=N` (1–100); sets `override_pct`, activates `STATE_OVERRIDE` |
| POST   | `/reset`    | Clears override, returns to normal polling mode |

### `/status` JSON Example

```json
{
  "wis_score": 14.5, "threshold": 147.2, "wis_pct": 9,
  "is_live": false, "mode": "off", "state": "IDLE"
}
```

### HTML Page (served inline)

Minimal, no external CSS/JS. Displays:
- **WIS score** and **threshold** (raw floats from last poll)
- **WIS %** — `wis_pct` as the primary status value (e.g., "9% of threshold")
- **Live status** — `is_live` from `mode` field (`mode` value shown in parentheses for debugging)
- **Current LED state**
- **Override slider** (1–100) and **Submit** button → POST `/override`
- **Reset** button → POST `/reset`

Auto-refresh every 30 seconds via `<meta http-equiv="refresh" content="30">`.

---

## 10. Dismiss Button (Optional)

- Uses `DISMISS_PIN` (GPIO 0 by default).
- On press during `STATE_LIVE` or `STATE_ALERT`: stop audio, transition to `STATE_IDLE`.
- Debounce with a 50ms `millis()` check in `loop()`.
- Dismissing does **not** lock out re-alerting — if `is_live` is still true on the next poll, the device transitions back to `STATE_ALERT`.

---

## 11. Main Loop (`src/main.cpp`)

```
setup():
  1. Init Serial
  2. Init LittleFS
  3. Init FastLED — set all LEDs to IDLE state
  4. Init WiFiManager (blocking until connected)
  5. Init WebServer
  7. Init Audio I2S
  8. Poll WIS immediately — populate wisData before first 60s elapses
  9. updateState(wisData)
 10. lastWisPollTime = millis()

loop():
  1. webServer.handleClient()
  2. audio.loop()
  3. handleDismissButton()
  5. runLedTick()                       // non-blocking: breath, strobe, bar fill
  6. if millis() - lastWisPollTime >= WIS_POLL_INTERVAL_MS:
         wisData = pollWIS()
         updateState(wisData)
         lastWisPollTime = millis()
```

---

## 12. State Transition Rules

```
Previous State   | is_live  | Next State
-----------------|----------|----------------------------------------------
STATE_IDLE       | true     | STATE_ALERT
STATE_ALERT      | (auto)   | STATE_LIVE  (after climb animation completes)
STATE_LIVE       | false    | STATE_IDLE
STATE_LIVE       | true     | STATE_LIVE  (no-op; bar fill updates in place)
STATE_IDLE       | false    | STATE_IDLE  (no-op; bar fill updates in place)
STATE_OVERRIDE   | any      | stays STATE_OVERRIDE until POST /reset
```

`wis_pct` updates on every successful poll regardless of state (except `STATE_OVERRIDE`, which uses `override_pct`).

---

## 13. LittleFS Audio Upload

Use PlatformIO's `Upload Filesystem Image` command (`pio run --target uploadfs`) to upload the `data/` folder containing `yall_live.mp3`.

The `.mp3` file should be:
- Mono or stereo, 44.1kHz or 22.05kHz
- 128kbps or lower bitrate to fit comfortably in flash

---

## 14. Open Questions / Decisions for Implementer

- [ ] **`mode` values**: Only `"off"` has been observed. Confirm what value appears when Ryan is live (e.g., `"live"`, `"on"`, `"active"`). The current spec treats anything that isn't `"off"` as live — log every observed `mode` value to Serial during testing.
- [ ] **Dismiss re-alert behavior**: Current spec allows re-alert on next poll if still live. Should dismiss lock out re-alerting for the full session instead?
- [ ] **WIS poll rate**: 60 seconds is conservative. 30 seconds is feasible and keeps the bar snappier — weigh against TLS connection overhead on the ESP32.
- [ ] **30m forecast**: `wis.weather_intensity_score_30m_from_now` is available. Could drive a single "rising/falling" indicator LED (e.g., LED 15 pulses orange if 30m forecast > current). Include or skip?
- [ ] **Threshold floor behavior**: Currently renders `wis_pct = 1` when `threshold < 10`. Alternative: blank the bar entirely on low-threshold days. Which is preferred?
- [ ] **OTA updates**: Add `ArduinoOTA` support for wireless firmware flashing?
- [ ] **YallCall alerts**: See `YALLCALL.md` for the full geofence alert feature spec (deferred, pending Ryan Hall publishing a YallCall API).
