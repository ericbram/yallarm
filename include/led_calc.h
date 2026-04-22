#pragma once
// Pure LED calculation functions — no Arduino/hardware dependencies.
// Used by src/leds.cpp and the native unit test suite.
#include <math.h>
#include "config.h"

enum ColorTier {
    TIER_BLUE    = 0,   // 1–COLOR_TIER_LOW %
    TIER_TEAL    = 1,   // COLOR_TIER_LOW+1 – COLOR_TIER_MID %
    TIER_MAGENTA = 2,   // COLOR_TIER_MID+1 – 100 %
};

// Number of bar LEDs to light for a given wis_pct (0–LED_BAR_COUNT).
inline int computeBarLeds(int wis_pct) {
    int n = (int)roundf((float)wis_pct / 10.0f);
    if (n < 0)             n = 0;
    if (n > LED_BAR_COUNT) n = LED_BAR_COUNT;
    return n;
}

// Color tier for the progress bar based on wis_pct.
inline ColorTier computeColorTier(int wis_pct) {
    if (wis_pct <= COLOR_TIER_LOW) return TIER_BLUE;
    if (wis_pct <= COLOR_TIER_MID) return TIER_TEAL;
    return TIER_MAGENTA;
}

// True when the top-two LEDs should strobe (score has reached threshold).
inline bool computeStrobe(int wis_pct) {
    return wis_pct >= 100;
}
