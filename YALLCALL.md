# YALLCALL.md — Future Feature Spec: GPS Location

This file captures the design spec for adding onboard GPS location to Y'all-ARM.
The feature has been deferred; the current firmware uses IP-based geolocation only.

---

## Overview

Add a hardware GPS module (NEO-6M / NEO-M8N / L76-style) as an optional second
location method. When enabled, GPS provides 2–5m accuracy outdoors and does not
depend on an internet lookup. When both GPS and IP geolocation are enabled, GPS is
preferred and IP acts as a fallback when no fresh fix is available.

**Design note:** A desk-bound device will show `valid = false` in GPS-only mode until
moved near a window or outdoors. If indoor coverage matters, enable IP geolocation
alongside GPS.

---

## Hardware

| Component  | Part                    | Notes                                      |
|------------|-------------------------|--------------------------------------------|
| GPS module | NEO-6M / NEO-M8N / L76 | UART, 3.3V logic; ~$8–15 on Amazon/AliExpress |

### Wiring

| Signal       | GPS pin | ESP32-S3 GPIO | Notes                          |
|--------------|---------|---------------|--------------------------------|
| GPS TX       | `TX`    | GPIO 17 (RX)  | GPS transmits → ESP32 receives |
| GPS RX       | `RX`    | GPIO 8 (TX)   | ESP32 transmits → GPS receives |
| Power        | `VCC`   | 3.3V          | **Use 3.3V — not 5V.** Most bare modules are 3.3V-only; wiring to 5V destroys them. Some breakouts include a regulator and accept 5V — confirm from the silkscreen before wiring. |
| Ground       | `GND`   | GND           | Common ground with ESP32, LEDs, amp |

**The 1Hz fix LED:** most GPS breakouts have a `PPS` or `FIX` LED. Solid = no fix.
Blinking once per second = fix acquired.

---

## Library

Add to `platformio.ini` `lib_deps` when GPS is enabled:

```ini
mikalhart/TinyGPSPlus @ ^1.0.3
```

Standard ESP32 library also required (no `lib_deps` entry needed):
- `HardwareSerial.h` (UART1)

---

## Configuration (`include/config.h`)

```cpp
// GPS module (UART1)
#define LOCATION_METHOD_GPS             1       // set to 1 to enable
#define GPS_UART_RX_PIN                 17
#define GPS_UART_TX_PIN                 8
#define GPS_UART_BAUD                   9600
#define GPS_FIX_STALE_MS                (5UL * 60UL * 1000UL)   // 5m

// When BOTH methods are enabled, fall back to IP if GPS has no fresh fix
#define LOCATION_GPS_TO_IP_FALLBACK_MS  (2UL * 60UL * 1000UL)   // 2m
```

When both `LOCATION_METHOD_IP_GEOLOCATION` and `LOCATION_METHOD_GPS` are enabled,
GPS wins when a fresh fix is available; IP cache is used otherwise.

---

## Operating Modes

| IP | GPS | Behavior                                                                    |
|----|-----|-----------------------------------------------------------------------------|
| 0  | 0   | Location disabled. `location` key omitted from `/status`.                   |
| 1  | 0   | IP only. City-level accuracy, refreshed every `IP_GEOLOCATION_REFRESH_MS`. |
| 0  | 1   | GPS only. 2–5m accuracy outdoors; `valid = false` until first fix.          |
| 1  | 1   | GPS preferred; IP fallback when GPS has no fix younger than `LOCATION_GPS_TO_IP_FALLBACK_MS`. |

---

## Types

Extend the existing `LocationSource` enum and `LocationData` struct in
`include/location.h`:

```cpp
enum LocationSource { LOC_SOURCE_NONE, LOC_SOURCE_IP, LOC_SOURCE_GPS };

struct LocationData {
    float          lat;
    float          lon;
    String         city;         // IP-based only; empty for GPS
    String         region;       // IP-based only; empty for GPS
    String         country;      // IP-based only; empty for GPS
    String         timezone;     // IP-based only; empty for GPS
    LocationSource source;
    uint32_t       last_update_ms;
    bool           valid;
};
```

---

## GPS Flow (`src/location.cpp`)

Active when `LOCATION_METHOD_GPS == 1`.

1. On `initLocation()`:
   - Instantiate a module-private `HardwareSerial Serial1(1)` and `TinyGPSPlus gps`.
   - Call `Serial1.begin(GPS_UART_BAUD, SERIAL_8N1, GPS_UART_RX_PIN, GPS_UART_TX_PIN)`.
   - Leave `currentLocation.valid = false` until the first valid fix arrives.
   - Log `[gps] Searching for fix…` to Serial.

2. `locationTick()` — runs every `loop()` iteration, non-blocking:
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

3. A fix is **stale** when `gps.location.age() > GPS_FIX_STALE_MS`. Stale fixes do
   not overwrite `currentLocation`; when GPS is the active source, `valid` is set to
   `false` on the next tick until a fresh fix arrives.

---

## Priority Resolution (Both Methods Enabled)

Evaluated in `locationTick()` after GPS bytes are serviced and after any scheduled IP
refresh:

```
if (GPS has a fix AND gps.location.age() <= LOCATION_GPS_TO_IP_FALLBACK_MS):
    currentLocation.source = LOC_SOURCE_GPS
else if (IP cache is valid):
    currentLocation.source = LOC_SOURCE_IP
else:
    currentLocation.valid  = false
    currentLocation.source = LOC_SOURCE_NONE
```

Emit a `Serial.printf` log line whenever `source` changes.

---

## `/status` JSON — GPS Source

When GPS is the active source, `city`, `region`, `country`, and `timezone` are empty
strings. `source` is `"gps"`.

```json
{
  "wis_score": 14.5, "threshold": 147.2, "wis_pct": 9,
  "is_live": false, "mode": "off", "state": "IDLE",
  "location": {
    "lat": 35.2271, "lon": -80.8431,
    "city": "", "region": "", "country": "", "timezone": "",
    "source": "gps",
    "valid": true
  }
}
```

---

## Testability

- With GPS only: confirm `valid = false` before first fix, `true` after, and `false`
  again once `gps.location.age() > GPS_FIX_STALE_MS`.
- With both enabled: unplug the GPS antenna and confirm `source` transitions from
  `"gps"` to `"ip"` within `LOCATION_GPS_TO_IP_FALLBACK_MS`.
- Compile with each of the four IP/GPS combinations and confirm the `/status` JSON
  matches the mode table above.

---

## Open Questions

- [ ] **Indoor use:** Should the device show a "no fix" indicator on the dashboard
  when GPS is enabled but has no valid fix? Or silently fall back to IP?
- [ ] **Cold-start UX:** Cold start to first fix takes 30–60 seconds outdoors. Should
  the dashboard show a "acquiring GPS fix…" status during that window?
- [ ] **Enclosure antenna:** The device lives on a desk. A ceramic patch antenna
  (included with most modules) may not get a fix indoors. Worth speccing an external
  active antenna mount in the enclosure design?
