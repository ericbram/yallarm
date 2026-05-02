#include "audio.h"
#include "config.h"
#include <Audio.h>
#include <LittleFS.h>

static Audio audio;

void audioInit() {
    audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
    audio.setVolume(AUDIO_VOLUME);
}

void audioPlayAlert() {
    audio.connecttoFS(LittleFS, AUDIO_FILE);
}

void audioStop() {
    audio.stopSong();
}

void audioLoop() {
    audio.loop();
}
