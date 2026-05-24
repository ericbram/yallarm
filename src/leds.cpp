#include "leds.h"
#include "config.h"
#include "led_calc.h"
#include <FastLED.h>

static CRGB leds[NUM_LEDS];

LedState ledState = STATE_IDLE;

static int      currentWisPct = 1;
static int      animStep      = 0;
static uint32_t animLastMs    = 0;
static uint32_t strobeLastMs  = 0;
static bool     strobeOn      = false;

// Independent overrides — see leds.h
static bool barOverrideActive  = false;
static int  barOverridePct     = 50;
static bool logoOverrideActive = false;
static bool logoOverrideOn     = true;

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
    // Logo: explicit override wins, otherwise state-driven
    if (logoOverrideActive) {
        fillLogo(logoOverrideOn ? CRGB::White : CRGB::Black);
    } else {
        fillLogo(ledState == STATE_IDLE ? CRGB::Black : CRGB::White);
    }

    // Bar: override wins; otherwise climb during ALERT, else live %
    if (barOverrideActive) {
        fillBar(barOverridePct);
        applyStrobe(barOverridePct);
    } else if (ledState == STATE_ALERT) {
        tickClimb();   // advances animStep, transitions to STATE_LIVE when done
    } else {
        fillBar(currentWisPct);
        applyStrobe(currentWisPct);
    }

    FastLED.show();
}

// ---------------------------------------------------------------------------
// Overrides
// ---------------------------------------------------------------------------

void ledsSetBarOverride(int pct) {
    barOverridePct    = constrain(pct, 1, 100);
    barOverrideActive = true;
}
void ledsClearBarOverride()  { barOverrideActive = false; }
bool ledsHasBarOverride()    { return barOverrideActive; }
int  ledsBarOverridePct()    { return barOverridePct; }

void ledsSetLogoOverride(bool on) {
    logoOverrideOn     = on;
    logoOverrideActive = true;
}
void ledsClearLogoOverride() { logoOverrideActive = false; }
bool ledsHasLogoOverride()   { return logoOverrideActive; }
bool ledsLogoOverrideOn()    { return logoOverrideOn; }

void ledsClearAllOverrides() {
    barOverrideActive  = false;
    logoOverrideActive = false;
}
bool ledsAnyOverride() { return barOverrideActive || logoOverrideActive; }

#ifdef LED_DIAG
void ledsDiagLoop() {
    // 6 bands of 20 LEDs each. If your strip dies at the cut, you'll see only
    // the red band — the second section starts at LED 20 (green band).
    static const CRGB bandColors[6] = {
        CRGB(80, 0,  0 ),   //   0– 19  red
        CRGB(0,  80, 0 ),   //  20– 39  green
        CRGB(0,  0,  80),   //  40– 59  blue
        CRGB(80, 80, 80),   //  60– 79  white
        CRGB(80, 80, 0 ),   //  80– 99  yellow
        CRGB(80, 0,  80),   // 100–119  magenta
    };

    // Dim for marginal-power scenarios — 80/255 × brightness keeps current low.
    FastLED.setBrightness(64);

    Serial.println("[diag] LED_DIAG active — running hardware test");
    Serial.println("[diag] bands:   0-19 red | 20-39 grn | 40-59 blu");
    Serial.println("[diag]         60-79 wht | 80-99 yel | 100-119 mag");

    while (true) {
        // Phase 1: hold color bands for 8s so you can inspect/photograph
        Serial.println("[diag] phase 1: color bands (8s)");
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = bandColors[i / 20];
        }
        FastLED.show();
        delay(8000);

        // Phase 2: march a single white pixel down the whole strip
        Serial.println("[diag] phase 2: marching pixel");
        for (int i = 0; i < NUM_LEDS; i++) {
            FastLED.clear();
            leds[i] = CRGB(120, 120, 120);
            FastLED.show();
            Serial.printf("[diag] pixel %d\n", i);
            delay(80);
        }
    }
}
#endif
