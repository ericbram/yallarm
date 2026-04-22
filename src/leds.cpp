#include "leds.h"
#include "config.h"
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

static CRGB colorForPct(int pct) {
    if (pct <= COLOR_TIER_LOW) return CRGB(0,   0,   200);   // Blue
    if (pct <= COLOR_TIER_MID) return CRGB(0,   128, 128);   // Teal
    return                            CRGB(200, 0,   200);   // Magenta
}

static void fillBar(int pct) {
    int barLeds = (int)round(pct / 10.0f);
    barLeds = constrain(barLeds, 0, LED_BAR_COUNT);
    CRGB col = colorForPct(pct);
    for (int i = LED_BAR_START; i <= LED_BAR_END; i++) {
        leds[i] = ((i - LED_BAR_START) < barLeds) ? col : CRGB::Black;
    }
}

// Strobe the top 2 bar LEDs when pct has hit 100
static void applyStrobe(int pct) {
    if (pct < 100) return;
    uint32_t now = millis();
    if (now - strobeLastMs >= STROBE_INTERVAL_MS) {
        strobeLastMs = now;
        strobeOn = !strobeOn;
    }
    CRGB strobeColor = strobeOn ? CRGB(200, 0, 200) : CRGB::Black;
    leds[LED_BAR_END]     = strobeColor;
    leds[LED_BAR_END - 1] = strobeColor;
}

// ---------------------------------------------------------------------------
// Per-state tick functions
// ---------------------------------------------------------------------------

static void tickBreath() {
    float t = (float)(millis() % BREATH_PERIOD_MS) / (float)BREATH_PERIOD_MS;
    float s = (sinf(t * TWO_PI) + 1.0f) / 2.0f;
    uint8_t bright = BREATH_MIN + (uint8_t)(s * (BREATH_MAX - BREATH_MIN));
    for (int i = LED_LOGO_START; i <= LED_LOGO_END; i++) {
        leds[i] = CRGB(0, bright, bright);
    }
}

static void tickClimb() {
    // 3 passes × 10 LEDs = 30 steps: Blue pass, then Teal, then Magenta
    static const CRGB passColors[3] = {
        CRGB(0,   0,   200),
        CRGB(0,   128, 128),
        CRGB(200, 0,   200),
    };

    uint32_t now = millis();
    if (now - animLastMs < CLIMB_STEP_MS) return;
    animLastMs = now;

    if (animStep < 30) {
        int pass = animStep / 10;
        int idx  = LED_BAR_START + (animStep % 10);
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
    currentWisPct = constrain(pct, 1, 100);
}

void ledsTick() {
    switch (ledState) {

        case STATE_IDLE:
            tickBreath();
            fillBar(currentWisPct);
            applyStrobe(currentWisPct);
            break;

        case STATE_ALERT:
            // Logo pulses in sync with the climb step timer
            {
                bool logoOn = ((millis() / CLIMB_STEP_MS) % 2 == 0);
                CRGB logoColor = logoOn ? CRGB(0, 128, 128) : CRGB::Black;
                for (int i = LED_LOGO_START; i <= LED_LOGO_END; i++) {
                    leds[i] = logoColor;
                }
            }
            tickClimb();    // advances one LED step every CLIMB_STEP_MS
            break;

        case STATE_LIVE:
            for (int i = LED_LOGO_START; i <= LED_LOGO_END; i++) {
                leds[i] = CRGB(0, 128, 128);
            }
            fillBar(currentWisPct);
            applyStrobe(currentWisPct);
            break;

        case STATE_OVERRIDE:
            for (int i = LED_LOGO_START; i <= LED_LOGO_END; i++) {
                leds[i] = CRGB(0, 128, 128);
            }
            fillBar(overridePct);
            applyStrobe(overridePct);
            break;
    }

    FastLED.show();
}
