#pragma once

void audioInit();
void audioPlayAlert();              // plays AUDIO_FILE from LittleFS
void audioPlayUrl(const char* url); // streams MP3 from an HTTP(S) URL
void audioStop();
void audioLoop();   // call every loop()
