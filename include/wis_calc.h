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

// FALLBACK live detection from wis.json's todays_stream_info.mode.
// `mode` is the *planned* stream posture for the day ("live" = active coverage
// expected today), not an on-air indicator — the site renders mode=="live" as
// "GOING LIVE SOON". Used only when the channel endpoint is unreachable.
inline bool computeIsLive(const char* mode) {
    if (mode == nullptr) return false;
    return strcmp(mode, "live") == 0;
}

// PRIMARY live detection from ryan_hall_yall.json's streams.current_live.
// Mirrors the site's own logic: not live when current_live is null; when
// present, trust its is_live boolean if set, otherwise treat a non-empty
// url or title as live.
inline bool computeChannelLive(bool current_live_present,
                               bool has_is_live_field, bool is_live_value,
                               bool has_url_or_title) {
    if (!current_live_present) return false;
    if (has_is_live_field) return is_live_value;
    return has_url_or_title;
}
