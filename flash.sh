#!/usr/bin/env bash
# Build, flash, and open the serial monitor in one shot.
#
# Usage:
#   ./flash.sh           # upload firmware + monitor
#   ./flash.sh fs        # upload LittleFS image (audio file) + monitor
#   ./flash.sh both      # firmware then filesystem, then monitor

set -euo pipefail

cd "$(dirname "$0")"

ENV="esp32-s3-devkitc-1"
TARGET="${1:-firmware}"

case "$TARGET" in
    firmware|"")
        pio run -e "$ENV" --target upload
        ;;
    fs|filesystem|uploadfs)
        pio run -e "$ENV" --target uploadfs
        ;;
    both)
        pio run -e "$ENV" --target upload
        pio run -e "$ENV" --target uploadfs
        ;;
    *)
        echo "unknown target: $TARGET (expected: firmware | fs | both)" >&2
        exit 1
        ;;
esac

exec pio device monitor -e "$ENV"
