# =============================================================================
# yallarm — PlatformIO build + test container
#
# Supports two use-cases without any local PlatformIO installation:
#   1. Native unit tests:  docker run --rm yallarm pio test -e native -v
#   2. ESP32-S3 firmware:  docker run --rm yallarm pio run -e esp32-s3-devkitc-1
#
# PlatformIO caches toolchains/libs in ~/.platformio (≈600 MB for the ESP32
# toolchain on first run).  Mount a named volume there to avoid re-downloading
# on every container start:
#
#   docker run --rm \
#     -v pio-cache:/root/.platformio \
#     -v "$(pwd)":/workspace \
#     yallarm pio run -e esp32-s3-devkitc-1
#
# NOTE: Flash-over-USB from within Docker is intentionally out of scope.
# =============================================================================

# python:3.12-slim gives us a pinned, maintained Python base with a minimal
# Debian footprint. PlatformIO's native platform needs system GCC; the ESP32
# toolchain ships its own xtensa-esp32s3-elf-gcc, so system GCC is only
# required for the native env.
FROM python:3.12-slim

# ---------------------------------------------------------------------------
# System dependencies
# ---------------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Native platform: PlatformIO delegates to the system compiler
    gcc \
    g++ \
    # Required by PlatformIO's package manager and git-based lib_deps
    git \
    # ESP32 toolchain download + extraction needs these
    curl \
    ca-certificates \
    # zip/unzip used by some PlatformIO platform installers
    unzip \
    # Make is needed by some framework build scripts
    make \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# PlatformIO Core (CLI only — no IDE)
# ---------------------------------------------------------------------------
RUN pip install --no-cache-dir platformio

# ---------------------------------------------------------------------------
# Pre-warm the PlatformIO package registry index so the first build inside
# CI doesn't hit the network for index updates.  Platform/toolchain downloads
# still happen on first use; mount a volume to cache those across runs.
# ---------------------------------------------------------------------------
RUN pio system info

# ---------------------------------------------------------------------------
# Working directory — project source is bind-mounted here at runtime, but we
# copy it in during image build so the image is self-contained for CI use.
# ---------------------------------------------------------------------------
WORKDIR /workspace
COPY . .

# ---------------------------------------------------------------------------
# Pre-install PlatformIO dependencies for both environments so the image is
# fully self-contained (no outbound network needed at test/build time).
#
# This step is intentionally split:
#   - native first (fast, no toolchain download)
#   - esp32-s3 second (downloads xtensa-esp32s3-elf toolchain, ~600 MB)
#
# The .pio/build/ artifacts are produced during these steps; they are removed
# afterwards so the committed image only contains the cached packages, not
# stale build output. Actual builds are done at runtime against the mounted
# source tree.
# ---------------------------------------------------------------------------
RUN pio pkg install -e native
RUN pio pkg install -e esp32-s3-devkitc-1

# Clean build artifacts so the image doesn't contain stale .o/.elf files,
# but keep the downloaded packages in ~/.platformio.
RUN rm -rf /workspace/.pio

# ---------------------------------------------------------------------------
# Default command — run both native tests and firmware build.
# Override this when running the container for a single task.
# ---------------------------------------------------------------------------
CMD ["sh", "-c", "pio test -e native -v && pio run -e esp32-s3-devkitc-1"]
