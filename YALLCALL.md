# YALLCALL.md — Future Feature Spec: YallCall Geofence Alerts

This file captures the full design spec for the YallCall feature. It is deferred pending
Ryan Hall Y'all publishing a YallCall API. No code in this repo implements any of this yet.

---

## Concept

Ryan Hall Y'all issues "YallCall" severe weather alerts for specific geographic areas —
essentially a personal severe weather watch/warning tied to his stream. The idea here:
if Ryan ever exposes a YallCall API (e.g. `ryanhallyall.com/rhy/yallcall.json`) that
publishes active alerts with geofences, Y'all-ARM should:

1. Know its own location (via IP geolocation or GPS)
2. Poll the YallCall endpoint periodically
3. Check whether the device is inside the geofence of any active YallCall
4. If yes — trigger an audio alert and a distinct LED animation
5. Clear the alert when the YallCall expires or the device leaves the geofence

This is completely independent of the WIS live-stream alert. Both can fire simultaneously.

---

## 1. Hypothetical YallCall API

The API shape is speculative. This is the assumed format to design against; update
if/when Ryan publishes the real endpoint.

**Endpoint:**
```
GET https://ryanhallyall.com/rhy/yallcall.json
```

**Assumed response:**
```json
{
  "yallcalls": [
    {
      "id": "yc_20260424_001",
      "issued_at": "2026-04-24T18:00:00Z",
      "expires_at": "2026-04-24T22:00:00Z",
      "severity": "tornado_watch",
      "headline": "Tornado Watch — Central Tennessee",
      "geofence": {
        "type": "circle",
        "lat": 36.1627,
        "lon": -86.7816,
        "radius_km": 120.0
      }
    }
  ]
}
```

**Design decisions to confirm when API is real:**
- Will geofences be circles (center + radius) or polygons? Circles are far cheaper to
  check on-device (Haversine distance). Polygons require a ray-casting algorithm.
- Will there be multiple active YallCalls simultaneously?
- Is there an auth requirement, or is it open like the WIS endpoint?
- What is the expected update frequency?

**Assumed poll interval:** 5 minutes (same as WIS). Adjust once real cadence is known.

---

## 2. Location Methods

The device needs to know where it is to evaluate the geofence. Two methods are
available; they're independent and can be used together (GPS preferred, IP fallback).

### Method A — IP Geolocation (no extra hardware)

Queries a free public service over WiFi on boot and caches the result in flash. No
wiring, no extra parts, no API key. Accuracy is city-level (~1–50 km depending on ISP).

**Config (`include/config.h`):**
```cpp
#define LOCATION_METHOD_IP_GEOLOCATION  1       // 0 = disable
#define IP_GEOLOCATION_URL              "http://ip-api.com/json/?fields=lat,lon,city,regionName,country,timezone"
#define IP_GEOLOCATION_REFRESH_MS       (24UL * 60UL * 60UL * 1000UL)   // 24h
```

**Flow (`src/location.cpp`):**
1. On `initLocation()`:
   - Open NVS via `Preferences` (namespace `"yallarm"`, read/write).
   - Load cached `loc_lat`, `loc_lon`, `loc_city`, `loc_time`. If valid, seed
     `currentLocation` immediately (`source = LOC_SOURCE_IP`, `valid = true`).
   - If cache is missing or older than `IP_GEOLOCATION_REFRESH_MS`, trigger a fetch.
2. `fetchIpLocation()`:
   - `HTTPClient` GET `IP_GEOLOCATION_URL` (plain HTTP — free tier doesn't support HTTPS).
   - Parse with `StaticJsonDocument<256>`. Extract `lat`, `lon`, `city`, `regionName`
     (→ `region`), `country`, `timezone`.
   - On success: update `currentLocation`, persist to NVS, set `valid = true`.
   - On failure: keep cached values, log to Serial, retry on next scheduled refresh.
3. `locationTick()` triggers `fetchIpLocation()` when refresh interval has elapsed.

Rate limit: ip-api.com free tier is ~45 req/min. With 24h refresh this is never a concern.

### Method B — GPS Module (optional hardware)

Adds a NEO-6M / NEO-M8N / L76-style UART GPS module. Provides 2–5m accuracy outdoors.
Requires a clear sky view — essentially useless indoors without a window. Best for
portable/travel use or installations near a window.

**Additional hardware:**

| Component  | Part                    | Notes                                          |
|------------|-------------------------|------------------------------------------------|
| GPS module | NEO-6M / NEO-M8N / L76 | UART, 3.3V logic; ~$8–15 on Amazon/AliExpress |

**Wiring:**

| Signal  | GPS pin | ESP32-S3 GPIO | Notes                                                        |
|---------|---------|---------------|--------------------------------------------------------------|
| GPS TX  | `TX`    | GPIO 17 (RX)  | GPS transmits → ESP32 receives                               |
| GPS RX  | `RX`    | GPIO 8 (TX)   | ESP32 transmits → GPS receives                               |
| Power   | `VCC`   | 3.3V          | **3.3V only.** Most bare modules are 3.3V; 5V destroys them. |
| Ground  | `GND`   | GND           | Common ground with ESP32, LEDs, amp                          |

The `PPS`/`FIX` LED on the module blinks 1 Hz when a fix is acquired.

**Additional library (`platformio.ini`):**
```ini
lib_deps =
    ; ... existing entries ...
    mikalhart/TinyGPSPlus @ ^1.0.3
```

Standard ESP32 library also needed (no `lib_deps` entry): `HardwareSerial.h`

**Config (`include/config.h`):**
```cpp
#define LOCATION_METHOD_GPS             1       // 0 = disable
#define GPS_UART_RX_PIN                 17
#define GPS_UART_TX_PIN                 8
#define GPS_UART_BAUD                   9600
#define GPS_FIX_STALE_MS                (5UL * 60UL * 1000UL)   // 5m

// Fall back to IP if GPS has no fresh fix for this long (only when both enabled)
#define LOCATION_GPS_TO_IP_FALLBACK_MS  (2UL * 60UL * 1000UL)   // 2m
```

**Flow (`src/location.cpp`):**
1. On `initLocation()`:
   - Instantiate `HardwareSerial Serial1(1)` and `TinyGPSPlus gps`.
   - Call `Serial1.begin(GPS_UART_BAUD, SERIAL_8N1, GPS_UART_RX_PIN, GPS_UART_TX_PIN)`.
   - Set `currentLocation.valid = false` until first fix.
2. `locationTick()` — non-blocking, every `loop()`:
   ```cpp
   while (Serial1.available()) gps.encode(Serial1.read());
   if (gps.location.isValid() && gps.location.isUpdated()) {
       currentLocation.lat            = (float)gps.location.lat();
       currentLocation.lon            = (float)gps.location.lng();
       currentLocation.source         = LOC_SOURCE_GPS;
       currentLocation.last_update_ms = millis();
       currentLocation.valid          = true;
   }
   ```
3. Fix is **stale** when `gps.location.age() > GPS_FIX_STALE_MS`. Stale → `valid = false`.

### Priority Resolution (Both Methods Enabled)

```
if (GPS fix present AND age <= LOCATION_GPS_TO_IP_FALLBACK_MS):
    use GPS
else if (IP cache valid):
    use IP cache
else:
    valid = false
```

Log a Serial line whenever `source` changes.

---

## 3. Types

New files: `include/location.h`, `src/location.cpp`, `include/yallcall_alert.h`,
`src/yallcall_alert.cpp`.

### Location

```cpp
// include/location.h
enum LocationSource { LOC_SOURCE_NONE, LOC_SOURCE_IP, LOC_SOURCE_GPS };

struct LocationData {
    float          lat;
    float          lon;
    String         city;         // IP only; empty for GPS
    String         region;       // IP only; empty for GPS
    String         country;      // IP only; empty for GPS
    String         timezone;     // IP only; empty for GPS
    LocationSource source;
    uint32_t       last_update_ms;
    bool           valid;
};

void                initLocation();
void                locationTick();
const LocationData& getLocation();
bool                locationEnabled();
```

### YallCall

```cpp
// include/yallcall_alert.h
struct YallCallGeofence {
    float  lat;
    float  lon;
    float  radius_km;
};

struct YallCall {
    String         id;
    String         severity;
    String         headline;
    YallCallGeofence geofence;
    uint32_t       expires_epoch;
    bool           valid;
};

void  initYallCall();
void  yallCallTick();
bool  isYallCallActive();
```

---

## 4. Geofence Check

Uses the **Haversine formula** to compute the great-circle distance between the device
location and the YallCall center, then compares against `radius_km`.

```cpp
// Returns distance in km between two lat/lon points
float haversineKm(float lat1, float lon1, float lat2, float lon2) {
    const float R = 6371.0f;
    float dLat = (lat2 - lat1) * M_PI / 180.0f;
    float dLon = (lon2 - lon1) * M_PI / 180.0f;
    float a = sinf(dLat/2) * sinf(dLat/2)
            + cosf(lat1 * M_PI / 180.0f) * cosf(lat2 * M_PI / 180.0f)
            * sinf(dLon/2) * sinf(dLon/2);
    return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

bool isInsideGeofence(const LocationData& loc, const YallCallGeofence& fence) {
    if (!loc.valid) return false;
    return haversineKm(loc.lat, loc.lon, fence.lat, fence.lon) <= fence.radius_km;
}
```

If the API ever moves to polygon geofences, replace with a ray-casting
point-in-polygon check.

---

## 5. Alert Behavior

A YallCall alert is distinct from the WIS live-stream alert. Both can fire at once.

| Trigger               | Audio                   | LED                                              |
|-----------------------|-------------------------|--------------------------------------------------|
| WIS goes live         | `yall_live.mp3`         | Logo pulses teal; bar runs blue→teal→magenta climb |
| Inside active YallCall | `yallcall_alert.mp3`   | All 16 LEDs pulse red; logo strobes              |

**`data/yallcall_alert.mp3`** — user-supplied, same format requirements as
`yall_live.mp3` (MP3, ≤128kbps, 44.1kHz or 22.05kHz). Not included in the repo.

**LED behavior during active YallCall:**
- All logo LEDs (0–5): strobe red at 500ms interval
- All bar LEDs (6–15): slow red pulse (breath effect)
- On YallCall expiry or geofence exit: return to normal WIS state

**Dismiss button:** pressing during an active YallCall alert silences audio and
reduces LEDs to a dim steady red for the remainder of the YallCall. Does not suppress
re-alert if a new YallCall is issued.

---

## 6. Config Defines (full set)

```cpp
// ---- YallCall API ----
#define YALLCALL_API_URL            "https://ryanhallyall.com/rhy/yallcall.json"
#define YALLCALL_POLL_INTERVAL_MS   (5UL * 60UL * 1000UL)   // 5m
#define YALLCALL_AUDIO_FILE         "/yallcall_alert.mp3"

// ---- Location — IP geolocation ----
// #define LOCATION_METHOD_IP_GEOLOCATION  1
// #define IP_GEOLOCATION_URL              "http://ip-api.com/json/?fields=lat,lon,city,regionName,country,timezone"
// #define IP_GEOLOCATION_REFRESH_MS       (24UL * 60UL * 60UL * 1000UL)

// ---- Location — GPS module ----
// #define LOCATION_METHOD_GPS             1
// #define GPS_UART_RX_PIN                 17
// #define GPS_UART_TX_PIN                 8
// #define GPS_UART_BAUD                   9600
// #define GPS_FIX_STALE_MS                (5UL * 60UL * 1000UL)
// #define LOCATION_GPS_TO_IP_FALLBACK_MS  (2UL * 60UL * 1000UL)
```

---

## 7. Integration with Main Loop

```
setup():
  ...existing setup...
  initLocation();      // after WiFi connects
  initYallCall();      // after initLocation()

loop():
  ...existing loop...
  locationTick();
  yallCallTick();      // polls API; checks geofence; triggers/clears alert
```

`yallCallTick()` is a no-op if `locationEnabled()` returns false (prevents false
alerts when the device doesn't know where it is).

### `/status` JSON additions

```json
{
  "wis_score": 14.5, "threshold": 147.2, "wis_pct": 9,
  "is_live": false, "mode": "off", "state": "IDLE",
  "location": {
    "lat": 36.1627, "lon": -86.7816,
    "city": "Nashville", "region": "Tennessee", "country": "US",
    "source": "ip", "valid": true
  },
  "yallcall": {
    "active": true,
    "id": "yc_20260424_001",
    "severity": "tornado_watch",
    "headline": "Tornado Watch — Central Tennessee",
    "inside_geofence": true
  }
}
```

`yallcall` key is always present when the feature is enabled, regardless of whether
an alert is active. `active: false` means no current YallCall (or device outside geofence).

---

## 8. Open Questions

- [ ] **API shape:** Does the real YallCall API use circle geofences, polygons, or
  county FIPS codes? If counties, need a FIPS-to-polygon lookup table.
- [ ] **Multiple simultaneous YallCalls:** Should the device alert on any match, or
  only the highest severity?
- [ ] **Severity levels:** What severity values will the API produce? Should some
  levels (e.g. `tornado_warning`) override the dismiss button?
- [ ] **No-location fallback:** If neither location method is enabled, should YallCall
  polling be skipped entirely, or should the device alert on all active YallCalls
  regardless of geofence?
- [ ] **Dashboard:** Show the active YallCall headline on the web dashboard? Show
  distance to geofence center?
- [ ] **Audio file:** Ship a default `yallcall_alert.mp3` in the repo, or keep it
  user-supplied like `yall_live.mp3`?
- [ ] **Antenna / indoor accuracy:** City-level IP geolocation means a 50km radius
  error is possible. If a YallCall geofence radius is smaller than the IP location
  error, the geofence check is unreliable. Should there be a minimum geofence radius
  below which the device skips the check and alerts unconditionally?
