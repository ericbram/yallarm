#include "wis.h"
#include "config.h"
#include "wis_calc.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

WisData wisData = {0.0f, 0.0f, 0.0f, "off", 1, false, false, false};

// Fetch streams.current_live from ryan_hall_yall.json — the authoritative
// on-air signal (same source the website's "LIVE NOW" badge uses).
// Sets ok=false on any HTTP/parse failure so the caller can fall back.
static bool fetchChannelLive(bool& ok) {
    ok = false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, CHANNEL_API_URL);
    http.setTimeout(10000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[WIS] Channel HTTP %d — falling back to mode\n", code);
        http.end();
        return false;
    }

    JsonDocument filter;
    filter["streams"]["current_live"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();

    if (err) {
        Serial.printf("[WIS] Channel parse error: %s — falling back to mode\n", err.c_str());
        return false;
    }

    ok = true;
    JsonVariantConst cur = doc["streams"]["current_live"];
    if (cur.isNull()) return computeChannelLive(false, false, false, false);

    bool has_is_live = cur["is_live"].is<bool>();
    bool is_live_val = cur["is_live"] | false;
    const char* url   = cur["url"]   | (const char*)nullptr;
    const char* title = cur["title"] | (const char*)nullptr;
    bool has_url_or_title = (url && url[0]) || (title && title[0]);

    return computeChannelLive(true, has_is_live, is_live_val, has_url_or_title);
}

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
    result.wis_pct       = computeWisPct(result.current_score, result.threshold, WIS_THRESHOLD_FLOOR);

    // Live detection: the channel endpoint is authoritative; mode is only the
    // day's planned posture ("live" = stream expected, not necessarily on air).
    bool channelOk = false;
    bool channelLive = fetchChannelLive(channelOk);
    if (channelOk) {
        result.is_live           = channelLive;
        result.live_via_fallback = false;
    } else {
        result.is_live           = computeIsLive(result.mode.c_str());
        result.live_via_fallback = true;
    }
    result.valid = true;

    Serial.printf("[WIS] score=%.2f  threshold=%.2f  pct=%d%%  mode=%s  live=%s%s\n",
                  result.current_score, result.threshold, result.wis_pct,
                  result.mode.c_str(), result.is_live ? "YES" : "no",
                  result.live_via_fallback ? " (mode fallback)" : "");

    wisData = result;
    return result;
}
