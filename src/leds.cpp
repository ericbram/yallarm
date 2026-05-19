#include "leds.h"
#include "config.h"
#include "led_calc.h"
#include <FastLED.h>

static CRGB leds[NUM_LEDS];

LedState ledState    = STATE_IDLE;
int      overridePct = 50;

static int     currentWisPct = 1;
static int     animStep      = 0;
static uint32_t animLastMs   = 0;
static uint32_t strobeLastMs = 0;
static bool    strobeOn      = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void fillLogo(CRGB color) {
    for (int i = LED_LOGO_START; i <= LED_LOGO_END; i++) {
        leds[i] = color;
    }
}

static void fillBar(int pct) {
    int barLeds = computeBarLeds(pct);
    RgbColor c = colorForPct(pct);
    CRGB col(c.r, c.g, c.b);
    for (int i = LED_BAR_START; i <= LED_BAR_END; i++) {
        leds[i] = ((i - LED_BAR_START) < barLeds) ? col : CRGB::Black;
    }
}

// Strobe the top 2 bar LEDs when pct has hit 100
static void applyStrobe(int pct) {
    if (!computeStrobe(pct)) return;
    uint32_t now = millis();
    if (now - strobeLastMs >= STROBE_INTERVAL_MS) {
        strobeLastMs = now;
        strobeOn = !strobeOn;
    }
    CRGB strobeColor = strobeOn ? CRGB(200, 0, 0) : CRGB::Black;
    leds[LED_BAR_END]     = strobeColor;
    leds[LED_BAR_END - 1] = strobeColor;
}

static void tickClimb() {
    // 3 passes × LED_BAR_COUNT LEDs: green wave, then yellow, then red —
    // mirrors the green→red progression of the steady-state bar.
    static const CRGB passColors[3] = {
        CRGB(0,   200, 0),
        CRGB(200, 200, 0),
        CRGB(200, 0,   0),
    };

    uint32_t now = millis();
    if (now - animLastMs < CLIMB_STEP_MS) return;
    animLastMs = now;

    if (animStep < LED_BAR_COUNT * 3) {
        int pass = animStep / LED_BAR_COUNT;
        int idx  = LED_BAR_START + (animStep % LED_BAR_COUNT);
        leds[idx] = passColors[pass];
        animStep++;
    } else {
        // Animation done — settle into LIVE state
        ledsSetState(STATE_LIVE);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ledsInit() {
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(LED_BRIGHTNESS);
    FastLED.clear(true);
}

void ledsSetState(LedState newState) {
    if (newState == STATE_ALERT) {
        animStep   = 0;
        animLastMs = millis();
        FastLED.clear();
    }
    ledState = newState;
}

void ledsSetWisPct(int pct) {
    currentWisPct = pct < 1 ? 1 : (pct > 100 ? 100 : pct);
}

void ledsTick() {
    switch (ledState) {

        case STATE_IDLE:
            // Not live → logo fully off
            fillLogo(CRGB::Black);
            fillBar(currentWisPct);
            applyStrobe(currentWisPct);
            break;

        case STATE_ALERT:
            fillLogo(CRGB::White);
            tickClimb();    // advances one LED step every CLIMB_STEP_MS
            break;

        case STATE_LIVE:
            fillLogo(CRGB::White);
            fillBar(currentWisPct);
            applyStrobe(currentWisPct);
            break;

        case STATE_OVERRIDE:
            fillLogo(CRGB::White);
            fillBar(overridePct);
            applyStrobe(overridePct);
            break;
    }

    FastLED.show();
}
