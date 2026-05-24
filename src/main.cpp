#include <Arduino.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <WebServer.h>

#include "config.h"
#include "wis.h"
#include "leds.h"
#include "app_audio.h"

static WebServer  server(WEB_SERVER_PORT);
static uint32_t   lastWisPollMs  = 0;
static uint32_t   dismissLastMs  = 0;
static bool       dismissActive  = false;

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

static void updateState(const WisData& w) {
    if (ledsAnyOverride()) return;              // user has manual control — hands off

    ledsSetWisPct(w.wis_pct);

    if (w.is_live && ledState == STATE_IDLE) {
        // Just went live: trigger alert animation + audio
        ledsSetState(STATE_ALERT);
        audioPlayAlert();
    } else if (!w.is_live && (ledState == STATE_LIVE || ledState == STATE_ALERT)) {
        // Stream ended: return to idle
        audioStop();
        ledsSetState(STATE_IDLE);
    }
    // STATE_LIVE + is_live → no-op (bar fill updates in place via ledsSetWisPct)
    // STATE_IDLE + !is_live → no-op
}

// ---------------------------------------------------------------------------
// Dismiss button
// ---------------------------------------------------------------------------

static void handleDismissButton() {
    if (digitalRead(DISMISS_PIN) == LOW) {
        uint32_t now = millis();
        if (!dismissActive && (now - dismissLastMs > DISMISS_DEBOUNCE_MS)) {
            dismissActive = true;
            dismissLastMs = now;
            if (ledState == STATE_LIVE || ledState == STATE_ALERT) {
                audioStop();
                ledsSetState(STATE_IDLE);
                Serial.println("[btn] Alert dismissed");
            }
        }
    } else {
        dismissActive = false;
    }
}

// ---------------------------------------------------------------------------
// Web dashboard
// ---------------------------------------------------------------------------

static const char* stateLabel() {
    switch (ledState) {
        case STATE_IDLE:  return "IDLE";
        case STATE_ALERT: return "ALERT";
        case STATE_LIVE:  return "LIVE";
        default:          return "UNKNOWN";
    }
}

static String barOverrideLabel() {
    return ledsHasBarOverride() ? "Override @ " + String(ledsBarOverridePct()) + "%" : "Auto";
}

static String logoOverrideLabel() {
    if (!ledsHasLogoOverride()) return "Auto";
    return ledsLogoOverrideOn() ? "Forced ON" : "Forced OFF";
}

static void handleRoot() {
    String live_class = wisData.is_live ? "live" : "offline";
    String live_text  = wisData.is_live ? "&#x1F7E2; LIVE" : "&#x26AB; Offline";

    String html =
        "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta http-equiv='refresh' content='30'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Y'all-ARM</title>"
        "<style>"
        "body{font-family:sans-serif;max-width:500px;margin:40px auto;padding:0 16px;color:#222}"
        "h1{font-size:1.5em;margin-bottom:4px}h2{font-size:1.1em;margin-top:24px}"
        "table{border-collapse:collapse;width:100%;margin-bottom:16px}"
        "td{padding:7px 10px;border-bottom:1px solid #eee}"
        "td:first-child{color:#666;width:45%}"
        ".live{color:#2a2;font-weight:bold}.offline{color:#999}"
        "input[type=range]{width:100%;margin:8px 0}"
        "button{padding:8px 18px;margin-top:6px;cursor:pointer;border:1px solid #ccc;"
        "border-radius:4px;background:#f5f5f5}"
        "button:hover{background:#e8e8e8}"
        "hr{border:none;border-top:1px solid #eee;margin:20px 0}"
        "</style></head><body>"
        "<h1>&#x1F4E1; Y'all-ARM</h1>";

    html += "<table>";
    html += "<tr><td>WIS Score</td><td>" + String(wisData.current_score, 2) + "</td></tr>";
    html += "<tr><td>Stream Threshold</td><td>" + String(wisData.threshold, 2) + "</td></tr>";
    html += "<tr><td>WIS %</td><td><strong>" + String(wisData.wis_pct) + "%</strong></td></tr>";
    html += "<tr><td>30m Forecast</td><td>" + String(wisData.score_30m, 2) + "</td></tr>";
    html += "<tr><td>Mode</td><td>" + wisData.mode + "</td></tr>";
    html += "<tr><td>Status</td><td class='" + live_class + "'>" + live_text + "</td></tr>";
    html += "<tr><td>LED State</td><td>" + String(stateLabel()) + "</td></tr>";
    html += "<tr><td>Bar Override</td><td>" + barOverrideLabel() + "</td></tr>";
    html += "<tr><td>Logo Override</td><td>" + logoOverrideLabel() + "</td></tr>";
    html += "</table><hr>";

    int sliderVal = ledsHasBarOverride() ? ledsBarOverridePct() : 50;

    html += "<h2>Manual Override</h2>"
            "<p style='color:#666;font-size:.9em'>Bar and logo are controlled separately. "
            "Click Reset to clear all overrides and return to live data.</p>"
            "<h3 style='margin-top:16px;font-size:1em'>Bar</h3>"
            "<form method='POST' action='/override'>"
            "WIS % (1&ndash;100):&nbsp;"
            "<output id='val'>" + String(sliderVal) + "</output>%"
            "<input type='range' name='level' min='1' max='100' value='" + String(sliderVal) + "'"
            " oninput=\"document.getElementById('val').value=this.value\">"
            "<br><button type='submit'>Apply Bar Override</button>"
            "</form>"
            "<h3 style='margin-top:16px;font-size:1em'>ON AIR Logo</h3>"
            "<form method='POST' action='/logo-on' style='display:inline'>"
            "<button type='submit'>Force ON</button>"
            "</form>&nbsp;"
            "<form method='POST' action='/logo-off' style='display:inline'>"
            "<button type='submit'>Force OFF</button>"
            "</form>"
            "<hr style='margin-top:20px'>"
            "<form method='POST' action='/reset'>"
            "<button type='submit'>Reset to Live Data</button>"
            "</form>"
            "<hr><h2>Audio Test</h2>"
            "<p style='color:#666;font-size:.9em'>Stream an MP3 from a URL to verify the audio chain "
            "(I2S amp + speaker). Leave the URL blank to play the on-device alert file from LittleFS.</p>"
            "<form method='POST' action='/test-audio'>"
            "<input type='url' name='url' "
            "placeholder='https://example.com/sample.mp3' "
            "style='width:100%;padding:6px;box-sizing:border-box'>"
            "<button type='submit' style='margin-top:8px'>Play Audio</button>"
            "</form>"
            "<form method='POST' action='/test-audio-stop'>"
            "<button type='submit' style='margin-top:8px'>Stop</button>"
            "</form>"
            "</body></html>";

    server.send(200, "text/html", html);
}

static void handleStatus() {
    String json = "{";
    json += "\"wis_score\":"  + String(wisData.current_score, 2) + ",";
    json += "\"threshold\":"  + String(wisData.threshold, 2) + ",";
    json += "\"wis_pct\":"    + String(wisData.wis_pct) + ",";
    json += "\"score_30m\":"  + String(wisData.score_30m, 2) + ",";
    json += "\"is_live\":"    + String(wisData.is_live ? "true" : "false") + ",";
    json += "\"mode\":\""     + wisData.mode + "\",";
    json += "\"state\":\""    + String(stateLabel()) + "\",";
    json += "\"bar_override\":"  + String(ledsHasBarOverride() ? "true" : "false") + ",";
    json += "\"bar_override_pct\":" + String(ledsBarOverridePct()) + ",";
    json += "\"logo_override\":" + String(ledsHasLogoOverride() ? "true" : "false") + ",";
    json += "\"logo_override_on\":" + String(ledsLogoOverrideOn() ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
}

static void handleOverride() {
    if (server.hasArg("level")) {
        int pct = constrain(server.arg("level").toInt(), 1, 100);
        ledsSetBarOverride(pct);
        Serial.printf("[web] Bar override set to %d%%\n", pct);
    }
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

static void handleLogoOn() {
    ledsSetLogoOverride(true);
    Serial.println("[web] Logo override: forced ON");
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

static void handleLogoOff() {
    ledsSetLogoOverride(false);
    Serial.println("[web] Logo override: forced OFF");
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

static void handleReset() {
    ledsClearAllOverrides();
    // Snap back to the correct automatic state based on current live data
    ledsSetWisPct(wisData.wis_pct);
    ledsSetState(wisData.is_live ? STATE_LIVE : STATE_IDLE);
    Serial.println("[web] All overrides cleared — back to auto");
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

static void handleTestAudio() {
    String url = server.hasArg("url") ? server.arg("url") : String();
    url.trim();
    if (url.length() > 0) {
        Serial.printf("[web] Audio test: streaming %s\n", url.c_str());
        audioPlayUrl(url.c_str());
    } else {
        Serial.printf("[web] Audio test: playing local %s\n", AUDIO_FILE);
        audioPlayAlert();
    }
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

static void handleTestAudioStop() {
    audioStop();
    Serial.println("[web] Audio test: stopped");
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[boot] Y'all-ARM starting");

    if (!LittleFS.begin(true)) {
        Serial.println("[boot] LittleFS mount failed — audio unavailable");
    }

    ledsInit();
    pinMode(DISMISS_PIN, INPUT_PULLUP);

#ifdef LED_DIAG
    Serial.println("[boot] LED_DIAG flag set — skipping WiFi/web/audio, entering diag loop");
    ledsDiagLoop();   // never returns
#endif

#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("[wifi] Trying compile-time credentials for \"%s\"\n", WIFI_SSID);
    uint32_t wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) delay(500);
#endif
    if (WiFi.status() != WL_CONNECTED) {
        WiFiManager wm;
        wm.setConfigPortalTimeout(WIFI_AP_TIMEOUT_S);
        if (!wm.autoConnect(WIFI_AP_NAME)) {
            Serial.println("[wifi] Config portal timed out — restarting");
            ESP.restart();
        }
    }
    Serial.printf("[wifi] Connected  IP: %s\n", WiFi.localIP().toString().c_str());

    server.on("/",                HTTP_GET,  handleRoot);
    server.on("/status",          HTTP_GET,  handleStatus);
    server.on("/override",        HTTP_POST, handleOverride);
    server.on("/logo-on",         HTTP_POST, handleLogoOn);
    server.on("/logo-off",        HTTP_POST, handleLogoOff);
    server.on("/reset",           HTTP_POST, handleReset);
    server.on("/test-audio",      HTTP_POST, handleTestAudio);
    server.on("/test-audio-stop", HTTP_POST, handleTestAudioStop);
    server.begin();
    Serial.printf("[web] Dashboard → http://%s\n", WiFi.localIP().toString().c_str());

    audioInit();

    // Initial poll so the bar fills immediately on boot rather than waiting 60s
    WisData initial = pollWIS();
    updateState(initial);
    lastWisPollMs = millis();
}

void loop() {
    server.handleClient();
    audioLoop();
    handleDismissButton();
    ledsTick();

    if (millis() - lastWisPollMs >= WIS_POLL_INTERVAL_MS) {
        WisData fresh = pollWIS();
        updateState(fresh);
        lastWisPollMs = millis();
    }
}
