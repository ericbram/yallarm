#include "leds.h"
#include "config.h"
#include "led_calc.h"
#include <FastLED.h>

static CRGB logoLeds[LED_LOGO_COUNT];
static CRGB barLeds[LED_BAR_COUNT];

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
    for (int i = 0; i < LED_LOGO_COUNT; i++) {
        logoLeds[i] = color;
    }
}

static void fillBar(int pct) {
    int barLitCount = computeBarLeds(pct);
    RgbColor c = colorForPct(pct);
    CRGB col(c.r, c.g, c.b);
    for (int i = 0; i < LED_BAR_COUNT; i++) {
        barLeds[i] = (i < barLitCount) ? col : CRGB::Black;
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
    barLeds[LED_BAR_COUNT - 1] = strobeColor;
    barLeds[LED_BAR_COUNT - 2] = strobeColor;
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
        int idx  = animStep % LED_BAR_COUNT;
        barLeds[idx] = passColors[pass];
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
    FastLED.addLeds<WS2812B, LED_LOGO_PIN, GRB>(logoLeds, LED_LOGO_COUNT);
    FastLED.addLeds<WS2812B, LED_BAR_PIN,  GRB>(barLeds,  LED_BAR_COUNT);
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
    FastLED.setBrightness(64);

    Serial.println("[diag] LED_DIAG active — hardware test on two strips");
    Serial.printf("[diag] logo: pin %d, %d LEDs   bar: pin %d, %d LEDs\n",
                  LED_LOGO_PIN, LED_LOGO_COUNT, LED_BAR_PIN, LED_BAR_COUNT);

    while (true) {
        // Phase 1: logo red, bar green — confirms both strips are wired
        Serial.println("[diag] phase 1: logo=red, bar=green (8s)");
        for (int i = 0; i < LED_LOGO_COUNT; i++) logoLeds[i] = CRGB(80, 0,  0);
        for (int i = 0; i < LED_BAR_COUNT;  i++) barLeds[i]  = CRGB(0,  80, 0);
        FastLED.show();
        delay(8000);

        // Phase 2: march a white pixel through the logo strip
        Serial.println("[diag] phase 2a: marching pixel through LOGO");
        for (int i = 0; i < LED_LOGO_COUNT; i++) {
            FastLED.clear();
            logoLeds[i] = CRGB(120, 120, 120);
            FastLED.show();
            Serial.printf("[diag] logo pixel %d\n", i);
            delay(80);
        }

        // Phase 2: march a white pixel through the bar strip
        Serial.println("[diag] phase 2b: marching pixel through BAR");
        for (int i = 0; i < LED_BAR_COUNT; i++) {
            FastLED.clear();
            barLeds[i] = CRGB(120, 120, 120);
            FastLED.show();
            Serial.printf("[diag] bar pixel %d\n", i);
            delay(80);
        }
    }
}
#endif
