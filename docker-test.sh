#!/usr/bin/env bash
# Build the yallarm Docker image and run tests inside it.
# DEBUG mode is false by default but this script enables it — override with:
#   DEBUG=false ./docker-test.sh
set -euo pipefail

DEBUG=${DEBUG:-true}
IMAGE=yallarm:dev
WIS_URL="https://ryanhallyall.com/rhy/wis.json"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

log()  { echo "[docker-test] $*"; }
warn() { echo "[docker-test] WARNING: $*" >&2; }

# ---------------------------------------------------------------------------
# Debug diagnostics — fetch live WIS data and print current device state
# ---------------------------------------------------------------------------

print_wis_diagnostics() {
    log "Fetching WIS data from ${WIS_URL} ..."
    local json
    if ! json=$(curl -sf --max-time 5 "$WIS_URL"); then
        warn "Could not reach WIS API — skipping diagnostics"
        return
    fi

    python3 - "$json" <<'EOF'
import sys, json, math

raw = sys.argv[1]
d   = json.loads(raw)
wis = d.get("wis", {})

score     = wis.get("weather_intensity_score", 0.0)
score_30m = wis.get("weather_intensity_score_30m_from_now", 0.0)
stream    = wis.get("todays_stream_info", {})
threshold = stream.get("weather_intensity_score_threshold", 0.0)
mode      = stream.get("mode", "unknown")

floor = 10.0
if threshold < floor:
    pct = 1
else:
    pct = max(1, min(100, int((score / threshold) * 100)))

on_air = "NO" if mode == "off" else f"YES ({mode})"

bar_filled = round(pct / 10.0)
bar = "█" * bar_filled + "░" * (10 - bar_filled)

print()
print("  ┌─────────────────────────────┐")
print("  │     WIS DIAGNOSTICS         │")
print("  ├─────────────────────────────┤")
print(f"  │  Score      : {score:<14.2f} │")
print(f"  │  30m Fore.  : {score_30m:<14.2f} │")
print(f"  │  Threshold  : {threshold:<14.2f} │")
pct_str = str(pct) + "%"
print(f"  │  WIS %      : {pct_str:<14} │")
print(f"  │  Mode       : {mode:<14} │")
print(f"  │  On Air     : {on_air:<14} │")
print(f"  │  Bar  [{bar}] │")
print("  └─────────────────────────────┘")
print()
EOF
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if [ "$DEBUG" = "true" ]; then
    print_wis_diagnostics
fi

log "Building image: ${IMAGE}"
docker build -t "$IMAGE" .

log "Running tests + firmware build inside container ..."
echo
docker run --rm "$IMAGE"
echo
log "Done."
