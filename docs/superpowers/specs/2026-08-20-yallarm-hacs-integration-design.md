# Yall-ARM Home Assistant integration (HACS custom integration)

## Overview

Replace the hand-pasted `rest`/`rest_command` YAML approach
(`homeassistant/yallarm.yaml`) with a real Home Assistant custom
integration, installable through HACS as a custom repository pointed
at this repo. The integration talks to the device's existing HTTP
dashboard (`src/main.cpp`) — no firmware changes.

## Goals

- Add the device via the HA UI (host/IP), no YAML editing.
- Fix the DHCP-drift problem hit on 2026-08-20 (device came up on a
  new IP after reflash) via an options flow that lets the host be
  changed without removing/re-adding the integration.
- One shared poll of `/status` backing all entities via
  `DataUpdateCoordinator`, instead of N independent per-entity fetches.
- Entities go `unavailable` cleanly when the device is unreachable,
  rather than erroring.
- All entities grouped under one device card.

## Non-goals

- No translations beyond English (`strings.json` only).
- No automated test suite — this is a personal single-device
  integration; manual verification against the real device is enough.
- Not submitted to the public HACS default store — added as a HACS
  custom repository by URL.
- No push/websocket updates — the firmware has no such mechanism;
  polling is the only option.

## Repo layout

```
custom_components/yallarm/
  __init__.py          # entry setup/unload, creates the coordinator
  manifest.json
  const.py             # DOMAIN, default poll interval
  config_flow.py        # setup flow (host) + options flow (change host)
  coordinator.py        # DataUpdateCoordinator, GET /status
  sensor.py
  binary_sensor.py
  switch.py
  number.py
  button.py
  strings.json
hacs.json               # repo root, category: integration
```

`custom_components/` must be at the repo root for HACS to recognize
this as an integration repository — the previously-created
`homeassistant/` folder is removed as part of this change.

`manifest.json`: `domain: yallarm`, `config_flow: true`,
`iot_class: local_polling`, no extra `requirements` (uses HA's bundled
`aiohttp` client).

## Config flow

- **Setup step**: single field, `host` (IP or hostname). Validates by
  calling `GET http://<host>/status` before accepting; on failure,
  shows an inline error and lets the user retry.
- **Unique ID**: the config entry's own `entry_id` — stable regardless
  of IP changes, so later host edits never orphan entities/history.
- **Options flow**: single field, `host`, editable any time via the
  integration's "Configure" button. Same validation as setup.

## Coordinator

`DataUpdateCoordinator` with a 30s update interval, one
`GET /status` per cycle, parsed into a plain dict. On request failure
(timeout, connection refused, non-200), raises `UpdateFailed` — HA's
standard mechanism turns this into "unavailable" on every entity
without custom per-entity error handling.

Device info: one `DeviceInfo` per config entry (`identifiers:
{(DOMAIN, entry.entry_id)}`, name "Yall-ARM"), attached to every
entity so they group under a single device card.

## Entities

All entities read from the shared coordinator's last-fetched dict;
writes call the corresponding POST endpoint then request a coordinator
refresh so the UI reflects the new state immediately.

| Entity | Domain | Source field | Write endpoint |
|---|---|---|---|
| WIS Score | sensor | `wis_score` | — |
| WIS Percent | sensor | `wis_pct` | — |
| WIS 30m Forecast | sensor | `score_30m` | — |
| Threshold | sensor | `threshold` | — |
| Mode | sensor | `mode` | — |
| LED State | sensor | `state` | — |
| Live | binary_sensor | `is_live` | — |
| Dark Mode | switch | `dark_mode` | `/dark-mode-on`, `/dark-mode-off` |
| Power | switch | `power_on` | `/power-on`, `/power-off` |
| Logo Override | switch | `logo_override_on` | `/logo-on`, `/logo-off` |
| Opacity | number (0–100) | `opacity` | `/opacity` (`level=<n>`) |
| Bar Override | number (1–100) | `bar_override_pct` | `/override` (`level=<n>`) — setting this always activates the override, matching firmware behavior; there's no "auto" toggle on this entity |
| Reset | button | — | `/reset` |
| Test Audio | button | — | `/test-audio` |
| Test Audio Stop | button | — | `/test-audio-stop` |

`live_via_fallback` and `bar_override`/`logo_override` (the boolean
"is an override active" flags, as opposed to the value/target fields
above) are not surfaced as separate entities — low value for a
personal dashboard, can be added later if wanted.

## Teardown of the previously-staged approach

As part of this change:
- Delete `homeassistant/yallarm.yaml`.
- Remove the `input_text.yall_arm_host` helper (superseded by the
  options flow).
- Remove the `automation.yall_arm_push_opacity_to_device` automation
  (superseded by the native `number.opacity` entity's direct write).
- Rebuild the "Y'all-ARM" section on the `home-landing` dashboard
  against the new native entity IDs (`switch.*`, `number.*`,
  `binary_sensor.*`, `button.*` instead of the old
  `rest_command.yallarm_*` service calls).

## Testing / verification

No automated test suite (non-goal). Verification is manual, against
the real device on the local network:
1. Add the integration via HACS + the HA UI, confirm all entities
   populate from a real `/status` response.
2. Toggle each switch and number entity, confirm the device visibly
   reacts (dashboard, LEDs, or `/status` polled independently).
3. Press each button, confirm the corresponding action fires.
4. Change the host via the options flow, confirm entities keep working
   without a HA restart.
5. Power off the device, confirm entities go `unavailable`; power it
   back on, confirm they recover on the next poll.
