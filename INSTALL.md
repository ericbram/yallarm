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

### Software (required on your computer)

| Tool | Why | How to get it |
|------|-----|---------------|
| **Python 3.9+** | PlatformIO runs on Python. | `brew install python` on macOS, or python.org. |
| **PlatformIO Core** | Build + flash toolchain. | Install via the VS Code extension (easiest) **or** `pip install platformio`. |
| **VS Code** (recommended) | IDE with a one-click build/flash button. | code.visualstudio.com |
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

---

## 3. Install the Toolchain

### Option A — VS Code + PlatformIO extension (recommended)

1. Install [VS Code](https://code.visualstudio.com/).
2. Open VS Code → Extensions panel (⇧⌘X) → search **"PlatformIO IDE"** → Install.
3. Wait 2–5 minutes for it to finish installing the core and toolchain (watch the status bar — it'll say "Finish PlatformIO setup" when done). **Restart VS Code when prompted.**

### Option B — PlatformIO CLI only

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

### Open in VS Code (option A users)

```bash
code .
```

PlatformIO auto-detects `platformio.ini` and downloads the project's library dependencies (FastLED, ArduinoJson, ESP32-audioI2S, WiFiManager) — give it a minute. You'll see progress in the bottom-status-bar terminal.

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

### From VS Code

1. Click the PlatformIO alien icon in the left sidebar.
2. Under **Project Tasks → esp32-s3-devkitc-1 → General**, click **Upload**.
3. Watch the terminal. You should see `Writing at 0x...` progress, then `Hash of data verified.` and `Leaving...`.

### From the CLI

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

### From VS Code

Click **Monitor** in the PlatformIO Project Tasks.

### From the CLI

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

## 12. Troubleshooting Quick Reference

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

Check the serial monitor first for any unexplained behavior — the firmware prints useful state transitions there.

---

## 13. Next Steps

- **Customize colors / thresholds:** edit `include/config.h`, reflash.
- **Change the audio clip:** replace `data/yall_live.mp3`, run `uploadfs`.
- **Integrate with other things:** the `/status` endpoint returns JSON — hook it into Home Assistant, a Stream Deck, or whatever you like.

For implementation details and the full design spec, see `AGENTS.md`.
