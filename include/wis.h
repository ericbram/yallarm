#pragma once
#include <Arduino.h>

struct WisData {
    float   current_score;     // wis.weather_intensity_score
    float   score_30m;         // wis.weather_intensity_score_30m_from_now
    float   threshold;         // wis.todays_stream_info.weather_intensity_score_threshold
    String  mode;              // wis.todays_stream_info.mode ("off" = not live)
    int     wis_pct;           // computed: (current_score / threshold) * 100, clamped 1–100
    bool    is_live;           // computed: mode != "off"
    bool    valid;             // false if the last poll failed (stale data retained)
};

// Global cached result — updated on every successful poll
extern WisData wisData;

// Perform one HTTP poll. Returns updated data; also updates wisData global.
// On failure, returns previous wisData with valid=false.
WisData pollWIS();
