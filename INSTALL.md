# Y'all-ARM — Install Guide

A complete, from-scratch walkthrough to build, flash, and run Y'all-ARM on an ESP32-S3. If you've never used an ESP32 before, start at step 1 and follow straight through.

---

## 1. What You Need

### Hardware (required)

| Item | Specifics | Notes |
|------|-----------|-------|
| ESP32-S3 DevKitC-1 | Any ESP32-S3 DevKitC-1 variant (N8R2, N16R8, etc.) | The board this firmware targets. Boards with the WROOM-1 module work fine. |
| USB-C cable | **Data-capable** USB-C (not charge-only) | Many cheap cables are power-only and will appear to do nothing. If the board doesn't show up as a serial port, try a different cable first. |
| WS2812B LED strip | 16 LEDs, 5V, "NeoPixel-compatible" | Must support the 800 kHz WS2812 timing. SK6812 usually works too. |
| MAX98357A I2S amp | Breakout board (Adafruit #3006 or equivalent) | Drives a speaker directly from the ESP32. |
| Speaker | 4Ω or 8Ω, ≤3W | Anything small — a scrap PC speaker is fine. |
| 5V power supply | ≥2A, barrel or USB | USB from a laptop **can** work for bench testing, but 16 WS2812B LEDs at full white draw ~1A — use a real 5V supply for reliable audio + LEDs. |
| Jumper wires | Dupont M-F and M-M | For breadboarding. |
| Breadboard | Half-size is plenty | Optional but recommended. |

### Hardware (optional)

- **Tactile button** — wired to GPIO 0 (the onboard BOOT button works too; wiring an external one is only needed if your enclosure hides the DevKit).
- **3D-printed enclosure** — the "ON AIR" logo and bar diffuser. Not covered here.
- **GPS module** (NEO-6M, NEO-M8N, or L76-style UART module, ~$8–15 on Amazon/AliExpress) — **only needed if you want onboard GPS location via Method B in section 12.** Not needed for IP-based location (Method A) or for normal operation. Most breakouts include a small ceramic antenna; a dedicated external active antenna helps if the device lives indoors.

### Software (required on your computer)

| Tool | Why | How to get it |
|------|-----|---------------|
| **Python 3.9+** | PlatformIO runs on Python. | `brew install python` on macOS, or python.org. |
| **PlatformIO Core** | Build + flash toolchain. | `pip install platformio` |
| **USB-serial drivers** | So your OS sees the board. | See step 3 — usually not needed on modern macOS/Linux. |
| **Git** | To clone the repo. | Pre-installed on macOS; `apt install git` on Linux. |

---

## 2. Wire the Board

**Power everything off before wiring.** Unplug USB from the ESP32.

### Pin map

| Signal | ESP32-S3 GPIO | Connects to |
|--------|---------------|-------------|
| LED data in | GPIO 18 | WS2812B `DIN` (first pixel) |
| LED 5V | 5V rail | WS2812B `5V` |
| LED ground | GND | WS2812B `GND` |
| I2S BCLK | GPIO 14 | MAX98357A `BCLK` |
| I2S LRC (word select) | GPIO 15 | MAX98357A `LRC` |
| I2S DIN (data out) | GPIO 16 | MAX98357A `DIN` |
| Amp power | 5V | MAX98357A `VIN` |
| Amp ground | GND | MAX98357A `GND` |
| Speaker + | — | MAX98357A `+` |
| Speaker − | — | MAX98357A `−` |
| Dismiss button | GPIO 0 | Button → GND (onboard BOOT works) |

### Power notes

- **USB from laptop:** fine for flashing and low-brightness testing. At `LED_BRIGHTNESS=128` with 16 LEDs you'll pull ~500–700 mA peak, which is at the edge of what some laptop USB ports supply reliably.
- **External 5V supply:** feed it into the `5V` pin on the DevKit and tie grounds together. **Do not feed more than 5V into the `5V` pin** — it bypasses the USB regulator.
- **LEDs get their power from the 5V rail**, not from the `3V3` pin. Do not tie LEDs to 3.3V — they'll look dim and flicker.
- **Common ground is mandatory.** LED strip GND, amp GND, ESP32 GND, and PSU GND must all be connected.

### First-time wiring sanity check

With nothing flashed yet, before powering up:

1. Double-check 5V and GND are not shorted (meter in continuity mode).
2. Confirm the LED strip's `DIN` end is the one wired to GPIO 18 (strips have a direction arrow — data flows **with** the arrow).
3. The MAX98357A's `GAIN` pin can be left floating for 9dB (default). Tie it to GND for 12dB or VIN for 15dB if the speaker is too quiet.

### Optional: GPS module wiring (for onboard location)

Skip this entire subsection unless you bought a GPS module and want to use Method B in section 12. If you're only doing IP-based location (Method A) or don't care about location at all, the device works fine without any of this.

The GPS talks to the ESP32 over a UART serial link at 9600 baud. TX on one side goes to RX on the other side — it's easy to get backwards the first time, so take your time here.

| Signal | GPS pin | ESP32-S3 GPIO |
|--------|---------|---------------|
| GPS TX | `TX` | GPIO 17 (RX on ESP32) |
| GPS RX | `RX` | GPIO 8 (TX on ESP32) |
| Power | `VCC` | 3.3V |
| Ground | `GND` | GND |

**Voltage caveat — read this before wiring power:** `VCC` must be **3.3V** on most modules. A bare NEO-6M or NEO-M8N chip is 3.3V-only and wiring it to 5V will cook it. Some breakout boards include an onboard regulator and accept 5V input — **confirm your specific breakout before wiring to 5V**, either from the silkscreen or the seller's spec sheet. When in doubt, use 3.3V from the DevKit's `3V3` pin.

**Ground:** tie GPS `GND` to the same common ground bus as the ESP32, LED strip, and amp. Everything shares one ground.

**The 1Hz fix LED:** almost every GPS breakout has a tiny LED (usually labeled `PPS` or `FIX`) that does one of two things:

- **Solid on or off** → no satellite fix yet. Cold start takes 30–60 seconds outdoors with a clear sky view; indoors you may never get a fix unless you're near a window.
- **Blinks once per second** → fix acquired. This is your hardware-level confirmation that the module is alive and seeing satellites, before you even flash the firmware.

If the LED stays dark entirely, double-check 3.3V and GND and confirm the module isn't wired backwards.

---

## 3. Install the Toolchain

```bash
python3 -m pip install --user platformio

# Make sure ~/.local/bin (Linux) or ~/Library/Python/3.x/bin (macOS) is on PATH.
# Then verify:
pio --version
```

### USB drivers

- **macOS 11+ and modern Linux:** the ESP32-S3 DevKitC-1 uses the built-in USB-CDC, so no driver is usually needed. When you plug it in, a `/dev/cu.usbmodem*` (macOS) or `/dev/ttyACM*` (Linux) device appears.
- **If your board uses a CP210x or CH340 USB-serial chip instead of native USB** (some clone boards do):
  - CP210x: [Silicon Labs driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
  - CH340: [WCH driver](https://www.wch-ic.com/downloads/CH341SER_MAC_ZIP.html)
- **Windows:** PlatformIO will prompt to install drivers on first use.

### Verify the board is seen

```bash
pio device list
```

You should see an entry like `/dev/cu.usbmodem101` with description `USB JTAG/serial debug unit` or similar. If nothing shows up:

- Try a different USB cable (most common cause).
- Try a different USB port (avoid hubs for flashing).
- Press and hold the `BOOT` button, tap `RESET`, then release `BOOT` — this puts the chip in download mode on stubborn boards.

---

## 4. Get the Code

```bash
git clone https://github.com/ericbram/yallarm.git
cd yallarm
```

PlatformIO will download the project's library dependencies (FastLED, ArduinoJson, ESP32-audioI2S, WiFiManager) automatically on first build.

---

## 5. Add the Audio File

The repo does not ship with the alert MP3. You need to supply one.

1. Find or make an MP3 clip you want to play when Ryan goes live (a few seconds is ideal).
2. Name it exactly `yall_live.mp3`.
3. Place it in the `data/` directory at the repo root:

   ```
   yallarm/
   └── data/
       └── yall_live.mp3
   ```

**Format requirements** (the ESP32-audioI2S decoder is picky):

| Property | Value |
|----------|-------|
| Container | MP3 (CBR preferred) |
| Sample rate | 44.1 kHz or 22.05 kHz |
| Bitrate | ≤128 kbps |
| Channels | Mono or stereo |

If your source file is bigger or higher bitrate, convert it with `ffmpeg`:

```bash
ffmpeg -i input.mp3 -ar 44100 -b:a 128k -ac 2 data/yall_live.mp3
```

---

## 6. Build and Flash

With the board connected via USB:

```bash
# Compile + upload firmware
pio run --target upload

# Upload the audio file (LittleFS filesystem image)
pio run --target uploadfs
```

**You must run `uploadfs` separately** — `upload` only flashes code. Skipping `uploadfs` means the MP3 isn't on the chip, and the alert will be silent.

### Common flash problems

| Symptom | Fix |
|---------|-----|
| `A fatal error occurred: Failed to connect` | Hold BOOT, tap RESET, release BOOT, retry. |
| `Permission denied` on `/dev/tty...` (Linux) | `sudo usermod -a -G dialout $USER` then log out/in. |
| Upload works but nothing happens after | Open the serial monitor (next step) to see what the board is doing. |
| `No serial ports found` | Cable is power-only, or no drivers. See step 3. |

---

## 7. Watch It Boot

Open the serial monitor to see what the device is doing:

```bash
pio device monitor
```

(`Ctrl-C` to quit.) Baud is 115200, already set in `platformio.ini`.

You should see logs like:

```
[boot] Y'all-ARM starting
[wifi] No saved credentials — launching captive portal
[wifi] AP "YallARM-Setup" is up at 192.168.4.1
```

Keep the monitor open — you'll want the device IP in a minute.

---

## 8. First-Boot WiFi Setup

The first time the device boots (or after a factory reset), it creates its own WiFi network so you can tell it your home WiFi credentials.

1. On your phone or laptop, open WiFi settings.
2. Connect to **`YallARM-Setup`** (no password).
3. A captive portal should pop up automatically. If it doesn't, open a browser and go to `http://192.168.4.1`.
4. Tap **Configure WiFi**.
5. Select your home network from the list, enter the password, tap **Save**.
6. The device saves the credentials to flash and reboots.
7. In the serial monitor, you'll now see:

   ```
   [wifi] Connected to "YourNetwork" — IP 192.168.1.42
   [web] Dashboard available at http://192.168.1.42/
   ```

Write down that IP — that's your dashboard address.

---

## 9. Verify It's Working

Open `http://<device-ip>/` in a browser. You should see:

- Current WIS score and today's threshold
- WIS % bar mirroring what the LEDs are showing
- Live / Offline status
- A manual override slider

Things to check on the hardware:

- **Bar LEDs (6–15):** idle at low brightness, color set by the current WIS %.
- **Logo LEDs (0–5):** slow teal "breathing" pulse.
- **Audio test:** move the manual override slider to 100%, then use the dashboard's **Test Alert** button (if present) — you should hear the MP3. If not, check `AUDIO_VOLUME` in `include/config.h` and the GAIN pin on the MAX98357A.

---

## 10. Running It Live

Once it's flashed, the device is autonomous:

- Plug it into any 5V USB port or adapter.
- It auto-connects to saved WiFi on boot.
- It polls `ryanhallyall.com/rhy/wis.json` every 60 seconds.
- When Ryan's stream mode flips from `off` to live, the alert animation runs and the MP3 plays.
- Press the dismiss button (GPIO 0 / BOOT) to silence an active alert.

**No laptop needed after flashing** — pull USB from your computer, plug it into a wall adapter, and it runs standalone.

---

## 11. Reconfiguring

### Change WiFi networks

Hold the BOOT button on power-up (or use WiFiManager's reset flow from the dashboard if exposed). The device will re-broadcast `YallARM-Setup` — go back to step 8.

### Change settings

All tunable settings live in `include/config.h`: poll interval, LED pins, brightness, colors, audio volume, button pin, etc. After editing, re-run `pio run --target upload`. No `uploadfs` needed unless you changed the audio file.

### Update the audio clip

1. Replace `data/yall_live.mp3`.
2. Run `pio run --target uploadfs`.
3. No firmware reflash needed.

---

## 12. Enabling Location (Optional)

Out of the box, Y'all-ARM has no idea where it is. That's fine — it doesn't need to know to do its job. But you may want location anyway: future features may surface it on the dashboard, log it with events, or use it for timezone-aware animations, and the `/status` endpoint can expose it for anything else you build on top.

There are two ways to give the device a sense of place. They're mutually optional — you can run either alone, run both together (GPS preferred, IP fallback), or skip location entirely.

### Method A — IP geolocation (no extra hardware)

This is the easy default. The device makes one HTTP request on boot to a free public geolocation service and caches the result in flash. No wiring, no extra parts, no API key. Accuracy is city-level — anywhere from 1 to 50 km depending on what your ISP publishes for your IP block.

How it works:

- On boot, once WiFi is up, the firmware does a single `GET http://ip-api.com/json/?fields=lat,lon,city,regionName,country,timezone`.
- The response (lat, lon, city, region, country, timezone) is cached to NVS/LittleFS so the device has a location immediately on subsequent boots without hitting the internet.
- The cache is refreshed once per day.

To enable it, open `include/config.h` and flip the flag:

```c
// Location sensing
#define LOCATION_METHOD_IP_GEOLOCATION 1   // 0 = off, 1 = on
```

Reflash with `pio run --target upload`. That's it. No `uploadfs` needed. On the next boot, check the serial monitor for a line like `[location] IP geo: Austin, Texas, US (30.27, -97.74)` and you're done.

### Method B — GPS module (optional hardware)

More precise (2–5 m outdoors) and doesn't depend on an internet lookup, but it requires a small extra board and a view of the sky. Good for a window-sill install; not great for a basement.

**Prerequisites:**

1. **Hardware:** a NEO-6M, NEO-M8N, or L76-style UART GPS module wired up per the pin-map in section 2 under "Optional: GPS module wiring." If you haven't wired it yet, do that first.
2. **Library:** open `platformio.ini` and add the TinyGPSPlus library to `lib_deps`:

   ```ini
   lib_deps =
       ; ...existing entries...
       mikalhart/TinyGPSPlus @ ^1.0.3
   ```

3. **Config flag:** in `include/config.h`, enable GPS:

   ```c
   // Location sensing
   #define LOCATION_METHOD_GPS 1   // 0 = off, 1 = on
   ```

Then rebuild and reflash with `pio run --target upload`. PlatformIO will pull down TinyGPSPlus on the next build.

**Verify it works:**

- On boot, check the serial monitor for a line like `[gps] Searching for fix…`.
- Move the device near a window or outside and give it 30–60 seconds for a cold start (a few seconds for a warm start if it's been used recently).
- The module's onboard fix LED should start blinking once per second when it locks on.
- Open `http://<device-ip>/status` in a browser. Once the module gets a fix, you should see a `location` object populated with `lat`, `lon`, and `source: "gps"`. Before the fix lands, `lat`/`lon` will be absent or `null`.

**Running both at the same time:** if you enable both `LOCATION_METHOD_IP_GEOLOCATION` and `LOCATION_METHOD_GPS`, the firmware uses GPS when it has a fix and falls back to the cached IP geolocation otherwise. The `source` field in `/status` tells you which one is currently winning.

When either method is enabled, the `/status` JSON gains a `location` object — wire it into Home Assistant, a dashboard, or whatever you're building on top.

---

## 13. Troubleshooting Quick Reference

| Symptom | Likely cause |
|---------|--------------|
| LEDs never light | Wrong pin in `LED_DATA_PIN`, strip wired backward, no 5V. |
| Only the first LED lights white | Data line not connected, or strip is wired from the wrong end (no arrow match). |
| All LEDs flicker / random colors | Grounds not tied together, or underpowered 5V. |
| Web dashboard unreachable | Device didn't get an IP — check serial monitor; re-run WiFi setup. |
| MP3 never plays | `uploadfs` was skipped, file isn't named `yall_live.mp3`, or bitrate >128kbps. |
| Audio is distorted | Lower `AUDIO_VOLUME` in `config.h`, or reduce MAX98357A GAIN. |
| Device keeps rebooting | Brownout from insufficient 5V current — use a proper adapter, not a weak USB port. |
| Dashboard says "Offline" forever | ESP32 can't reach the internet — verify it's on your WiFi, not an IoT-isolated guest network. |
| Location shows as `null` or missing on dashboard | Neither location method is enabled in `config.h`, or the IP geolocation service rate-limited you (free tier allows 45 requests/min per IP — hitting it shouldn't happen in normal use, but can if you're reboot-looping). |
| GPS fix never arrives | Indoor install with no sky view, GPS antenna not plugged in (for modules with a separate antenna), or the module is running at the wrong baud rate — firmware expects 9600, which is the default for NEO-6M/NEO-M8N/L76. |
| GPS module gets hot or power LED flickers | Almost always `VCC` wired to 5V on a 3.3V-only module. Disconnect power immediately and move `VCC` to the `3V3` pin. |

Check the serial monitor first for any unexplained behavior — the firmware prints useful state transitions there.

---

## 14. Next Steps

- **Customize colors / thresholds:** edit `include/config.h`, reflash.
- **Change the audio clip:** replace `data/yall_live.mp3`, run `uploadfs`.
- **Integrate with other things:** the `/status` endpoint returns JSON — hook it into Home Assistant, a Stream Deck, or whatever you like.

For implementation details and the full design spec, see `AGENTS.md`.
