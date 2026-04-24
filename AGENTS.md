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
├── src/
│   ├── main.cpp           # Primary application entry point
│   └── location.cpp       # Location sensing implementation (optional)
├── data/
│   └── yall_live.mp3      # Audio alert file (stored in LittleFS)
├── include/
│   ├── config.h           # Pin definitions, constants, endpoint URL
│   ├── leds.h             # LED state machine declarations
│   ├── audio.h            # Audio playback declarations
│   ├── wis.h              # WIS API polling declarations
│   └── location.h         # Location sensing declarations (optional)
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
| Optional GPS      | NEO-6M / NEO-M8N / L76  | UART1 — GPS TX → GPIO 17 (RX), GPS RX → GPIO 8 (TX), VCC → 3V3, GND → GND |

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
    mikalhart/TinyGPSPlus @ ^1.0.3   ; (optional, only when LOCATION_METHOD_GPS is enabled)

board_build.filesystem = littlefs
```

Standard ESP32 libraries used (no `lib_deps` entry needed):
- `WiFi.h`
- `HTTPClient.h`
- `LittleFS.h`
- `WebServer.h`
- `HardwareSerial.h` (GPS UART, when `LOCATION_METHOD_GPS` is enabled)
- `Preferences.h` (NVS-backed cache for IP geolocation)

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

// Location (optional)
// Location — both are independently optional. If both enabled, GPS is
// preferred and IP acts as a fallback when no GPS fix for N minutes.
#define LOCATION_METHOD_IP_GEOLOCATION  1       // 0 = disable
#define LOCATION_METHOD_GPS             0       // 0 = disable

// IP geolocation
#define IP_GEOLOCATION_URL              "http://ip-api.com/json/?fields=lat,lon,city,regionName,country,timezone"
#define IP_GEOLOCATION_REFRESH_MS       (24UL * 60UL * 60UL * 1000UL)   // 24h

// GPS module (UART1)
#define GPS_UART_RX_PIN                 17
#define GPS_UART_TX_PIN                 8
#define GPS_UART_BAUD                   9600
#define GPS_FIX_STALE_MS                (5UL * 60UL * 1000UL)           // 5m

// When BOTH are enabled, fall back to IP if GPS has no fresh fix
#define LOCATION_GPS_TO_IP_FALLBACK_MS  (2UL * 60UL * 1000UL)           // 2m
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

## 8. Location (Optional)

Location sensing is fully optional. Two independent methods are supported and may be enabled together. When both are enabled, GPS is preferred and IP acts as a fallback when no fresh GPS fix is available.

### Files

- `include/location.h` — public API, enum and struct declarations
- `src/location.cpp` — implementation (IP fetch, GPS parse, cache, priority resolution)

### Operating Modes

Driven by the compile-time flags `LOCATION_METHOD_IP_GEOLOCATION` and `LOCATION_METHOD_GPS` in `config.h`:

| IP | GPS | Behavior                                                                 |
|----|-----|--------------------------------------------------------------------------|
| 0  | 0   | Location disabled. `location` key omitted from `/status`.                |
| 1  | 0   | IP only. City-level accuracy, refreshed every `IP_GEOLOCATION_REFRESH_MS`. |
| 0  | 1   | GPS only. 2–5m accuracy outdoors; `valid = false` until first fix.       |
| 1  | 1   | GPS preferred; IP fallback when GPS has no fix younger than `LOCATION_GPS_TO_IP_FALLBACK_MS`. |

**Design note:** A GPS module typically produces no fix indoors. A desk-bound device will therefore show `valid = false` in GPS-only mode until it is moved near a window or outdoors. If indoor coverage matters, enable IP as well.

### Types

```cpp
enum LocationSource { LOC_SOURCE_NONE, LOC_SOURCE_IP, LOC_SOURCE_GPS };

struct LocationData {
    float         lat;
    float         lon;
    String        city;          // IP-based only; empty for GPS
    String        region;        // IP-based only; empty for GPS
    String        country;       // IP-based only; empty for GPS
    String        timezone;      // IP-based only; empty for GPS
    LocationSource source;
    uint32_t      last_update_ms;
    bool          valid;
};
```

A module-private `LocationData currentLocation` holds the latest resolved fix; `getLocation()` returns a const reference.

### Public API (`include/location.h`)

```cpp
void                initLocation();      // call after WiFi connects in setup()
void                locationTick();      // call every loop() iteration
const LocationData& getLocation();       // read-only accessor
bool                locationEnabled();   // true if at least one method is enabled
```

When neither method is enabled, `initLocation()` and `locationTick()` are no-ops and `locationEnabled()` returns `false`.

### IP Geolocation Flow

Active when `LOCATION_METHOD_IP_GEOLOCATION == 1`.

1. On `initLocation()`:
   - Open NVS via `Preferences` (namespace `"yallarm"`, read/write).
   - Load keys `loc_lat`, `loc_lon`, `loc_city`, `loc_time` (timestamp of last successful fetch, stored as `uint32_t` seconds since epoch or `millis()` at cache time — implementer choice, document the choice in code).
   - If a cached value exists, seed `currentLocation` with it (`source = LOC_SOURCE_IP`, `valid = true`).
   - If the cache is missing or older than `IP_GEOLOCATION_REFRESH_MS`, trigger an immediate fetch.
2. `fetchIpLocation()`:
   - `HTTPClient` GET `IP_GEOLOCATION_URL` (plain HTTP — `ip-api.com` free tier does not support HTTPS).
   - Parse response body with `StaticJsonDocument<256>`.
   - Extract `lat` (float), `lon` (float), `city` (String), `regionName` (String, stored as `region`), `country` (String), `timezone` (String).
   - On success: update `currentLocation`, set `source = LOC_SOURCE_IP`, `valid = true`, `last_update_ms = millis()`. Persist `loc_lat`, `loc_lon`, `loc_city`, and the timestamp to NVS.
   - On HTTP error (non-200) or JSON parse failure: keep any previously cached values, leave `valid` at its current state (cached → `true`, no cache → `false`), log to Serial, and retry on the next scheduled refresh.
3. `locationTick()` triggers `fetchIpLocation()` when `millis() - lastIpFetchMs >= IP_GEOLOCATION_REFRESH_MS`.

Rate limit: `ip-api.com` free tier caps at ~45 req/min per source IP. With a 24h refresh, this is never a concern; do not poll faster than once per hour.

### GPS Flow

Active when `LOCATION_METHOD_GPS == 1`.

1. On `initLocation()`:
   - Instantiate a module-private `HardwareSerial Serial1(1)` and `TinyGPSPlus gps`.
   - Call `Serial1.begin(GPS_UART_BAUD, SERIAL_8N1, GPS_UART_RX_PIN, GPS_UART_TX_PIN)`.
   - Leave `currentLocation.valid = false` until the first valid fix arrives.
2. `locationTick()` (must run every `loop()` iteration, non-blocking):
   ```cpp
   while (Serial1.available()) {
       gps.encode(Serial1.read());
   }
   if (gps.location.isValid() && gps.location.isUpdated()) {
       currentLocation.lat            = (float)gps.location.lat();
       currentLocation.lon            = (float)gps.location.lng();
       currentLocation.city           = "";
       currentLocation.region         = "";
       currentLocation.country        = "";
       currentLocation.timezone       = "";
       currentLocation.source         = LOC_SOURCE_GPS;
       currentLocation.last_update_ms = millis();
       currentLocation.valid          = true;
   }
   ```
3. A GPS fix is considered **stale** when `gps.location.age() > GPS_FIX_STALE_MS`. Stale fixes do not overwrite `currentLocation` and, when GPS is the active source, cause `valid` to be set to `false` on the next tick until a fresh fix arrives.

### Priority Resolution (Both Methods Enabled)

Evaluated in `locationTick()` after GPS bytes are serviced and after any scheduled IP refresh:

```
if (GPS has a fix AND gps.location.age() <= LOCATION_GPS_TO_IP_FALLBACK_MS):
    currentLocation.source = LOC_SOURCE_GPS
    (populated from latest gps.location values)
else if (IP cache is valid):
    currentLocation.source = LOC_SOURCE_IP
    (populated from IP cache)
else:
    currentLocation.valid = false
    currentLocation.source = LOC_SOURCE_NONE
```

The switch between sources is evaluated on every tick — no hysteresis. A `Serial.printf` log line is emitted whenever `source` changes.

### Integration with Main Loop

- Add to `setup()` after WiFi connects (and before the first WIS poll): `initLocation();`
- Add to `loop()` right after `audio.loop()`: `locationTick();`

Both calls are safe when location is fully disabled (no-ops).

### Testability

Every behavior above must be individually verifiable:

- Compile with each of the four IP/GPS combinations and confirm the `/status` JSON matches the mode table.
- With IP only: disconnect WiFi after the first successful fetch and confirm the cached value continues to be served with `valid = true`.
- With GPS only: confirm `valid = false` before first fix, `true` after, and `false` again once `gps.location.age() > GPS_FIX_STALE_MS`.
- With both: unplug GPS antenna and confirm `source` transitions from `gps` to `ip` within `LOCATION_GPS_TO_IP_FALLBACK_MS`.

---

## 9. Web Dashboard

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
  "is_live": false, "mode": "off", "state": "IDLE",
  "location": {
    "lat": 37.7749, "lon": -122.4194,
    "city": "San Francisco", "region": "California", "country": "US",
    "timezone": "America/Los_Angeles",
    "source": "ip",
    "valid": true
  }
}
```

The `location` key is **only present** when at least one location method is enabled and has produced a valid result. When location is disabled, or no valid fix exists yet, omit the key entirely — do **not** emit `"location": null`. The `source` field is one of `"ip"` or `"gps"` (mapped from `LocationSource`; `LOC_SOURCE_NONE` means the key is omitted). For GPS-sourced fixes, `city`, `region`, `country`, and `timezone` are empty strings.

### HTML Page (served inline)

Minimal, no external CSS/JS. Displays:
- **WIS score** and **threshold** (raw floats from last poll)
- **WIS %** — `wis_pct` as the primary status value (e.g., "9% of threshold")
- **Live status** — `is_live` from `mode` field (`mode` value shown in parentheses for debugging)
- **Current LED state**
- **Location** (lat/lon, city, source) if configured
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
  5. initLocation()                     // no-op if both methods disabled
  6. Init WebServer
  7. Init Audio I2S
  8. Poll WIS immediately — populate wisData before first 60s elapses
  9. updateState(wisData)
 10. lastWisPollTime = millis()

loop():
  1. webServer.handleClient()
  2. audio.loop()
  3. locationTick()                     // services GPS bytes; triggers IP refresh
  4. handleDismissButton()
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
- [ ] **Location UI**: show a small city-name label or lat/lon coordinates on the dashboard, or only expose via `/status` JSON?
- [ ] **Location used for**: pure display, or feed into future location-aware WIS / regional alert features?
