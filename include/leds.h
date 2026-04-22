#pragma once
#include <Arduino.h>

enum LedState {
    STATE_IDLE,       // not live — logo breathes dim teal, bar shows real WIS %
    STATE_ALERT,      // just went live — logo pulses, bar runs climb animation
    STATE_LIVE,       // live and alert already played — logo solid, bar shows real WIS %
    STATE_OVERRIDE,   // manual override from web dashboard
};

extern LedState ledState;
extern int      overridePct;    // 1–100, active only in STATE_OVERRIDE

void ledsInit();
void ledsSetState(LedState newState);
void ledsSetWisPct(int pct);    // update current WIS % (used in IDLE + LIVE states)
void ledsTick();                // call every loop() — non-blocking
