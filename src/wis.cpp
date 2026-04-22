#include "wis.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

WisData wisData = {0.0f, 0.0f, 0.0f, "off", 1, false, false};

WisData pollWIS() {
    WisData result = wisData;   // start with cached values so a failed poll returns stale data
    result.valid = false;

    WiFiClientSecure client;
    // Certificate verification is skipped for simplicity — the WIS endpoint is
    // public read-only data with no sensitive payload.
    client.setInsecure();

    HTTPClient http;
    http.begin(client, WIS_API_URL);
    http.setTimeout(10000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[WIS] HTTP %d — using cached data\n", code);
        http.end();
        return result;
    }

    // Filter: only parse the fields we need. This skips score_history and
    // daily_outlook_scores (large arrays), keeping heap usage low.
    JsonDocument filter;
    filter["wis"]["weather_intensity_score"] = true;
    filter["wis"]["weather_intensity_score_30m_from_now"] = true;
    filter["wis"]["todays_stream_info"]["weather_intensity_score_threshold"] = true;
    filter["wis"]["todays_stream_info"]["mode"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();

    if (err) {
        Serial.printf("[WIS] Parse error: %s — using cached data\n", err.c_str());
        return result;
    }

    result.current_score = doc["wis"]["weather_intensity_score"] | 0.0f;
    result.score_30m     = doc["wis"]["weather_intensity_score_30m_from_now"] | 0.0f;
    result.threshold     = doc["wis"]["todays_stream_info"]["weather_intensity_score_threshold"] | 0.0f;
    result.mode          = doc["wis"]["todays_stream_info"]["mode"] | "off";
    result.is_live       = (result.mode != "off");
    result.valid         = true;

    // Compute wis_pct: score as a percentage of today's stream threshold, clamped 1–100
    if (result.threshold < WIS_THRESHOLD_FLOOR) {
        result.wis_pct = 1;
    } else {
        float raw = (result.current_score / result.threshold) * 100.0f;
        result.wis_pct = (int)constrain(raw, 1.0f, 100.0f);
    }

    Serial.printf("[WIS] score=%.2f  threshold=%.2f  pct=%d%%  mode=%s  live=%s\n",
                  result.current_score, result.threshold, result.wis_pct,
                  result.mode.c_str(), result.is_live ? "YES" : "no");

    wisData = result;
    return result;
}
