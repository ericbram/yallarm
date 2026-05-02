#pragma once

// =============================================================================
// NETWORK
// =============================================================================
// Optional compile-time WiFi credentials — useful during development to skip
// the captive portal on every reflash. Put real values in `include/secrets.h`
// (gitignored — copy `secrets.h.example` to start). If the file is absent or
// the credentials don't connect, the device falls back to the captive portal.
#if __has_include("secrets.h")
    #include "secrets.h"
#endif

// Captive portal settings (used when compile-time credentials are absent or fail)
#define WIFI_AP_NAME        "YallARM-Setup"
#define WIFI_AP_TIMEOUT_S   180             // seconds before AP times out and device restarts

// =============================================================================
// WIS API
// =============================================================================
#define WIS_API_URL             "https://ryanhallyall.com/rhy/wis.json"
#define WIS_POLL_INTERVAL_MS    (300 * 1000)    // how often to poll, in milliseconds
#define WIS_THRESHOLD_FLOOR     10.0f           // if threshold < this, bar stays at minimum
                                                // (avoids false "maxed out" on slow weather days)

// =============================================================================
// LED HARDWARE
// =============================================================================
#define LED_DATA_PIN        18
#define NUM_LEDS            16
#define LED_BAR_COUNT       10
#define LED_LOGO_START      0
#define LED_LOGO_END        5
#define LED_BAR_START       6
#define LED_BAR_END         15

// =============================================================================
// LED VISUAL SETTINGS
// =============================================================================
#define LED_BRIGHTNESS          128     // global brightness 0–255
#define BREATH_MIN              5       // dimmest point of idle breath
#define BREATH_MAX              40      // brightest point of idle breath
#define BREATH_PERIOD_MS        3000    // duration of one full breath cycle
#define CLIMB_STEP_MS           80      // ms between each LED step in alert animation
#define STROBE_INTERVAL_MS      250     // ms between strobe flashes when score >= threshold

// Progress bar color tiers (by wis_pct value)
#define COLOR_TIER_LOW          40      // 1–40%  → Blue
#define COLOR_TIER_MID          70      // 41–70% → Teal
                                        // 71–100% → Magenta

// =============================================================================
// AUDIO — I2S / MAX98357A
// =============================================================================
#define I2S_BCLK_PIN        15
#define I2S_LRC_PIN         16
#define I2S_DOUT_PIN        17
#define AUDIO_FILE          "/yall_live.mp3"    // path inside LittleFS (data/ folder)
#define AUDIO_VOLUME        15                  // 0 (silent) to 21 (max)

// =============================================================================
// DISMISS BUTTON
// =============================================================================
// GPIO 4 — safe input pin on the ESP32-S3-DevKitC-1. Avoids the strapping pins
// (0, 3, 45, 46) that affect boot mode. Wire the button between GPIO 4 and GND;
// INPUT_PULLUP is enabled in firmware, so no external resistor is needed.
#define DISMISS_PIN             4
#define DISMISS_DEBOUNCE_MS     50

// =============================================================================
// WEB DASHBOARD
// =============================================================================
#define WEB_SERVER_PORT     80
