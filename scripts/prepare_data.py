# Copies the default alert MP3 from samples/ into data/ before the LittleFS
# image is built, so `pio run -t uploadfs` always ships a working audio file
# even on a fresh checkout. data/yall_live.mp3 is .gitignored — samples/ is
# the source of truth.

import os, shutil

Import("env")  # type: ignore  # noqa: F821  -- injected by PlatformIO

ROOT   = env["PROJECT_DIR"]                                # type: ignore  # noqa: F821
SAMPLE = os.path.join(ROOT, "samples",  "weather.mp3")
TARGET = os.path.join(ROOT, "data",     "yall_live.mp3")

if os.path.exists(SAMPLE):
    os.makedirs(os.path.dirname(TARGET), exist_ok=True)
    shutil.copy2(SAMPLE, TARGET)
    print(f"[prepare_data] {SAMPLE} -> {TARGET}")
else:
    print(f"[prepare_data] WARNING: {SAMPLE} not found; LittleFS image will lack /yall_live.mp3")
