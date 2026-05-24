#pragma once
#include <Arduino.h>

enum LedState {
    STATE_IDLE,       // not live — logo off, bar shows real WIS %
    STATE_ALERT,      // just went live — logo on, bar runs climb animation
    STATE_LIVE,       // live (alert played) — logo on, bar shows real WIS %
};

extern LedState ledState;

void ledsInit();
void ledsSetState(LedState newState);
void ledsSetWisPct(int pct);    // update current WIS % (used in IDLE + LIVE states)
void ledsTick();                // call every loop() — non-blocking

// Manual overrides — independent. Each can be set on its own; both are
// cleared by ledsClearAllOverrides(). While *any* override is active,
// automatic stream-driven state transitions and alert audio are suspended.
void ledsSetBarOverride(int pct);     // pin bar to a fixed % (1..100)
void ledsClearBarOverride();
bool ledsHasBarOverride();
int  ledsBarOverridePct();

void ledsSetLogoOverride(bool on);    // pin ON AIR logo ON or OFF
void ledsClearLogoOverride();
bool ledsHasLogoOverride();
bool ledsLogoOverrideOn();

void ledsClearAllOverrides();
bool ledsAnyOverride();

#ifdef LED_DIAG
// Hardware diagnostic: 6 color bands across all 120 LEDs, then a marching
// single pixel with serial output. Never returns. Build with -DLED_DIAG.
void ledsDiagLoop();
#endif
