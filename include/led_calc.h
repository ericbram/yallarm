#pragma once
// Pure LED calculation functions — no Arduino/hardware dependencies.
// Used by src/leds.cpp and the native unit test suite.
#include <math.h>
#include <stdint.h>
#include "config.h"

struct RgbColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// Number of bar LEDs to light for a given wis_pct (0–LED_BAR_COUNT).
inline int computeBarLeds(int wis_pct) {
    int n = (int)roundf((float)wis_pct * LED_BAR_COUNT / 100.0f);
    if (n < 0)             n = 0;
    if (n > LED_BAR_COUNT) n = LED_BAR_COUNT;
    return n;
}

// Progress-bar color for wis_pct: green at 1% → yellow at 50% → red at 100%.
// Two-segment lerp through yellow keeps the midpoint bright (vs. a direct
// green↔red lerp, which dims to olive at 50%).
inline RgbColor colorForPct(int wis_pct) {
    if (wis_pct < 1)   wis_pct = 1;
    if (wis_pct > 100) wis_pct = 100;
    uint8_t r, g;
    if (wis_pct < 50) {
        r = (uint8_t)(wis_pct * 4);                 // 0 → 200
        g = 200;
    } else {
        r = 200;
        g = (uint8_t)(200 - (wis_pct - 50) * 4);    // 200 → 0
    }
    return {r, g, 0};
}

// True when the top-two LEDs should strobe (score has reached threshold).
inline bool computeStrobe(int wis_pct) {
    return wis_pct >= 100;
}
