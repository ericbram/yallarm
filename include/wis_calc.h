#pragma once
// Pure WIS calculation functions — no Arduino/hardware dependencies.
// Used by src/wis.cpp and the native unit test suite.
#include <string.h>

// Returns wis_pct: score expressed as a percentage of threshold, clamped 1–100.
// Returns 1 if threshold is below the floor (avoids divide-by-zero / false maxing).
inline int computeWisPct(float score, float threshold, float threshold_floor) {
    if (threshold < threshold_floor) return 1;
    float raw = (score / threshold) * 100.0f;
    if (raw < 1.0f)   return 1;
    if (raw > 100.0f) return 100;
    return (int)raw;
}

// Returns true when the stream is considered live.
// "off" is the only known non-live value; any other mode is treated as live.
inline bool computeIsLive(const char* mode) {
    if (mode == nullptr) return false;
    return strcmp(mode, "off") != 0;
}
