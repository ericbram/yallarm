# Yall-ARM HACS Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `custom_components/yallarm/`, a real Home Assistant integration installable via HACS, replacing the hand-pasted `rest`/`rest_command` YAML approach.

**Architecture:** A config flow collects the device host and validates it against a live `GET /status`. One `DataUpdateCoordinator` polls `/status` every 30s and backs `sensor`/`binary_sensor` (read-only) and `switch`/`number`/`button` (write, via POST to the device's existing endpoints) entities. An options flow lets the host be changed later without removing the integration.

**Tech Stack:** Python (Home Assistant custom integration conventions), `aiohttp` (via HA's shared client session helper), HACS (custom repository, category `integration`).

**Spec:** `docs/superpowers/specs/2026-08-20-yallarm-hacs-integration-design.md`

## Global Constraints

- Domain is `yallarm`. Device host: currently `192.168.0.117` (drifts on reflash — this project's whole reason to exist).
- No automated test suite (spec non-goal). Per-file tasks verify with a syntax check only; real behavior is verified live against the actual device and the actual HA instance in Task 11/12, using the Home Assistant MCP tools already connected in this session (`ha_manage_hacs`, `ha_set_integration`, `ha_search`, `ha_get_integration`).
- No translations beyond English (`strings.json` only, no `translations/` subdir).
- Repo remote is `git@github-personal:ericbram/yallarm.git`; work happens directly on `main` (established pattern this session — build, test, commit, push, no PR).
- `custom_components/` must be at the repo root for HACS's integration category — not nested under `homeassistant/`.
- Every write entity (switch/number/button) calls `coordinator.async_request_refresh()` after its POST so the UI reflects the device's new state immediately, matching how the coordinator is the single source of truth for all read state.

---

### Task 1: Package scaffolding

**Files:**
- Create: `hacs.json`
- Create: `custom_components/yallarm/manifest.json`
- Create: `custom_components/yallarm/const.py`
- Create: `custom_components/yallarm/strings.json`

**Interfaces:**
- Produces: `const.DOMAIN = "yallarm"`, `const.DEFAULT_SCAN_INTERVAL = 30` — every later task imports these from `.const`.

- [ ] **Step 1: Create `hacs.json`**

```json
{
  "name": "Yall-ARM",
  "render_readme": true
}
```

- [ ] **Step 2: Create `custom_components/yallarm/manifest.json`**

```json
{
  "domain": "yallarm",
  "name": "Yall-ARM",
  "codeowners": ["@ericbram"],
  "config_flow": true,
  "documentation": "https://github.com/ericbram/yallarm",
  "iot_class": "local_polling",
  "issue_tracker": "https://github.com/ericbram/yallarm/issues",
  "requirements": [],
  "version": "0.1.0"
}
```

- [ ] **Step 3: Create `custom_components/yallarm/const.py`**

```python
DOMAIN = "yallarm"
DEFAULT_SCAN_INTERVAL = 30
```

- [ ] **Step 4: Create `custom_components/yallarm/strings.json`**

```json
{
  "config": {
    "step": {
      "user": {
        "title": "Connect to Yall-ARM",
        "description": "Enter the IP address or hostname of your Yall-ARM device (no http://).",
        "data": {
          "host": "Host"
        }
      }
    },
    "error": {
      "cannot_connect": "Could not reach the device at that address."
    }
  },
  "options": {
    "step": {
      "init": {
        "title": "Yall-ARM host",
        "data": {
          "host": "Host"
        }
      }
    },
    "error": {
      "cannot_connect": "Could not reach the device at that address."
    }
  }
}
```

- [ ] **Step 5: Verify all three JSON files parse and `const.py` imports cleanly**

Run:
```bash
cd ~/code/yallarm
python3 -c "import json; [json.load(open(f)) for f in ['hacs.json', 'custom_components/yallarm/manifest.json', 'custom_components/yallarm/strings.json']]; print('json ok')"
python3 -c "import ast; ast.parse(open('custom_components/yallarm/const.py').read()); print('const.py ok')"
```
Expected: `json ok` then `const.py ok`, no errors.

- [ ] **Step 6: Commit**

```bash
cd ~/code/yallarm
git add hacs.json custom_components/yallarm/manifest.json custom_components/yallarm/const.py custom_components/yallarm/strings.json
git commit -m "Add HACS integration scaffolding for yallarm"
```

---

### Task 2: API client (`api.py`)

**Files:**
- Create: `custom_components/yallarm/api.py`

**Interfaces:**
- Consumes: nothing project-local (only `aiohttp`, stdlib `asyncio`).
- Produces: `YallarmApiError(Exception)`, `async def async_get_status(session: aiohttp.ClientSession, host: str) -> dict`, `async def async_post(session: aiohttp.ClientSession, host: str, path: str, data: dict | None = None) -> None` — every later task (`coordinator.py`, `config_flow.py`, `switch.py`, `number.py`, `button.py`) imports these three names from `.api`.

- [ ] **Step 1: Write `custom_components/yallarm/api.py`**

```python
"""Thin async HTTP client for the yallarm device's built-in web dashboard."""
from __future__ import annotations

import asyncio

import aiohttp


class YallarmApiError(Exception):
    """Raised when the yallarm device cannot be reached or returns an error."""


async def async_get_status(session: aiohttp.ClientSession, host: str) -> dict:
    """Fetch and parse GET /status from the device."""
    try:
        async with asyncio.timeout(10):
            resp = await session.get(f"http://{host}/status")
            if resp.status != 200:
                raise YallarmApiError(
                    f"unexpected status {resp.status} from {host}/status"
                )
            return await resp.json()
    except (aiohttp.ClientError, asyncio.TimeoutError) as err:
        raise YallarmApiError(f"could not reach yallarm device at {host}") from err


async def async_post(
    session: aiohttp.ClientSession,
    host: str,
    path: str,
    data: dict | None = None,
) -> None:
    """POST to one of the device's action endpoints (e.g. /power-on, /opacity)."""
    try:
        async with asyncio.timeout(10):
            resp = await session.post(
                f"http://{host}{path}", data=data, allow_redirects=False
            )
            if resp.status not in (200, 302):
                raise YallarmApiError(
                    f"unexpected status {resp.status} posting {path} to {host}"
                )
    except (aiohttp.ClientError, asyncio.TimeoutError) as err:
        raise YallarmApiError(f"could not reach yallarm device at {host}") from err
```

- [ ] **Step 2: Syntax check**

Run: `cd ~/code/yallarm && python3 -c "import ast; ast.parse(open('custom_components/yallarm/api.py').read()); print('api.py ok')"`
Expected: `api.py ok`, no errors.

- [ ] **Step 3: Verify the request logic against the real device**

The device is reachable at `192.168.0.117`. Run this standalone script (mirrors `api.py`'s logic exactly, using plain `requests` so it runs outside HA) to confirm the shape of a real response and that both helpers behave correctly:

```bash
python3 - <<'EOF'
import requests

host = "192.168.0.117"

status = requests.get(f"http://{host}/status", timeout=10).json()
expected_keys = {
    "wis_score", "threshold", "wis_pct", "score_30m", "is_live",
    "live_via_fallback", "mode", "state", "bar_override", "bar_override_pct",
    "logo_override", "logo_override_on", "opacity", "dark_mode", "power_on",
}
missing = expected_keys - status.keys()
assert not missing, f"missing keys: {missing}"
print("GET /status ok:", status)

resp = requests.post(f"http://{host}/power-on", timeout=10, allow_redirects=False)
assert resp.status_code in (200, 302), resp.status_code
print("POST /power-on ok:", resp.status_code)

bad = requests.Session()
try:
    requests.get("http://192.168.0.254/status", timeout=2)
    print("WARNING: expected a connection error against an unused IP")
except requests.exceptions.RequestException:
    print("unreachable-host case raises as expected")
EOF
```
Expected: `GET /status ok: {...}` with all expected keys present, `POST /power-on ok: 302` (or `200`), and the unreachable-host case prints the "raises as expected" line.

- [ ] **Step 4: Commit**

```bash
cd ~/code/yallarm
git add custom_components/yallarm/api.py
git commit -m "Add yallarm API client (GET /status, POST actions)"
```

---

### Task 3: Coordinator (`coordinator.py`)

**Files:**
- Create: `custom_components/yallarm/coordinator.py`

**Interfaces:**
- Consumes: `api.async_get_status`, `api.YallarmApiError`, `const.DOMAIN`, `const.DEFAULT_SCAN_INTERVAL`.
- Produces: `class YallarmCoordinator(DataUpdateCoordinator[dict])` with a public mutable `.host: str` attribute (read/written by `__init__.py`'s update listener when the options flow changes the host) and inherited `.data: dict`, `.async_request_refresh()`, `.async_config_entry_first_refresh()`. Every entity platform and `__init__.py` imports `YallarmCoordinator` from `.coordinator`.

- [ ] **Step 1: Write `custom_components/yallarm/coordinator.py`**

```python
"""DataUpdateCoordinator for the yallarm device."""
from __future__ import annotations

from datetime import timedelta
import logging

from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

from .api import YallarmApiError, async_get_status
from .const import DEFAULT_SCAN_INTERVAL, DOMAIN

_LOGGER = logging.getLogger(__name__)


class YallarmCoordinator(DataUpdateCoordinator[dict]):
    """Polls GET /status on the yallarm device and shares it with all entities."""

    def __init__(self, hass: HomeAssistant, host: str) -> None:
        super().__init__(
            hass,
            _LOGGER,
            name=DOMAIN,
            update_interval=timedelta(seconds=DEFAULT_SCAN_INTERVAL),
        )
        self.host = host

    async def _async_update_data(self) -> dict:
        session = async_get_clientsession(self.hass)
        try:
            return await async_get_status(session, self.host)
        except YallarmApiError as err:
            raise UpdateFailed(str(err)) from err
```

- [ ] **Step 2: Syntax check**

Run: `cd ~/code/yallarm && python3 -c "import ast; ast.parse(open('custom_components/yallarm/coordinator.py').read()); print('coordinator.py ok')"`
Expected: `coordinator.py ok`, no errors. (Full behavioral verification happens live in Task 11/12 — a `DataUpdateCoordinator` needs a real `hass` object to instantiate, which isn't available outside a running Home Assistant.)

- [ ] **Step 3: Commit**

```bash
cd ~/code/yallarm
git add custom_components/yallarm/coordinator.py
git commit -m "Add yallarm DataUpdateCoordinator"
```

---

### Task 4: Shared entity base (`entity.py`)

**Files:**
- Create: `custom_components/yallarm/entity.py`

**Interfaces:**
- Consumes: `const.DOMAIN`, `coordinator.YallarmCoordinator`.
- Produces: `class YallarmEntity(CoordinatorEntity[YallarmCoordinator])` whose `__init__(self, coordinator, entry_id)` sets `self._attr_device_info` (grouping every entity under one device card) — every platform's entity class in Tasks 6-9 subclasses this alongside its HA platform mixin (e.g. `class YallarmSensor(YallarmEntity, SensorEntity)`).

- [ ] **Step 1: Write `custom_components/yallarm/entity.py`**

```python
"""Shared base entity: device grouping for every yallarm entity."""
from __future__ import annotations

from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import YallarmCoordinator


class YallarmEntity(CoordinatorEntity[YallarmCoordinator]):
    """Base class giving every yallarm entity a shared device and naming style."""

    _attr_has_entity_name = True

    def __init__(self, coordinator: YallarmCoordinator, entry_id: str) -> None:
        super().__init__(coordinator)
        self._attr_device_info = DeviceInfo(
            identifiers={(DOMAIN, entry_id)},
            name="Yall-ARM",
        )
```

- [ ] **Step 2: Syntax check**

Run: `cd ~/code/yallarm && python3 -c "import ast; ast.parse(open('custom_components/yallarm/entity.py').read()); print('entity.py ok')"`
Expected: `entity.py ok`, no errors.

- [ ] **Step 3: Commit**

```bash
cd ~/code/yallarm
git add custom_components/yallarm/entity.py
git commit -m "Add shared yallarm entity base class"
```

---

### Task 5: Config flow (`config_flow.py`)

**Files:**
- Create: `custom_components/yallarm/config_flow.py`

**Interfaces:**
- Consumes: `api.async_get_status`, `api.YallarmApiError`, `const.DOMAIN`.
- Produces: `class YallarmConfigFlow(config_entries.ConfigFlow, domain=DOMAIN)`, `class YallarmOptionsFlow(config_entries.OptionsFlow)`. Both are discovered by Home Assistant via `manifest.json`'s `"config_flow": true` plus this module living at `custom_components/yallarm/config_flow.py` — no other file references these classes directly.

- [ ] **Step 1: Write `custom_components/yallarm/config_flow.py`**

```python
"""Config + options flow: collect and validate the device host."""
from __future__ import annotations

from typing import Any

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.core import callback
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .api import YallarmApiError, async_get_status
from .const import DOMAIN

DATA_SCHEMA = vol.Schema({vol.Required("host"): str})


async def _validate_host(hass, host: str) -> None:
    session = async_get_clientsession(hass)
    await async_get_status(session, host)


class YallarmConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle initial setup: ask for the host, validate it, create the entry."""

    VERSION = 1

    async def async_step_user(
        self, user_input: dict[str, Any] | None = None
    ) -> config_entries.ConfigFlowResult:
        errors: dict[str, str] = {}
        if user_input is not None:
            try:
                await _validate_host(self.hass, user_input["host"])
            except YallarmApiError:
                errors["base"] = "cannot_connect"
            else:
                return self.async_create_entry(title="Yall-ARM", data=user_input)
        return self.async_show_form(
            step_id="user", data_schema=DATA_SCHEMA, errors=errors
        )

    @staticmethod
    @callback
    def async_get_options_flow(
        config_entry: config_entries.ConfigEntry,
    ) -> YallarmOptionsFlow:
        return YallarmOptionsFlow(config_entry)


class YallarmOptionsFlow(config_entries.OptionsFlow):
    """Let the host be changed later without removing the integration."""

    def __init__(self, config_entry: config_entries.ConfigEntry) -> None:
        self.config_entry = config_entry

    async def async_step_init(
        self, user_input: dict[str, Any] | None = None
    ) -> config_entries.ConfigFlowResult:
        errors: dict[str, str] = {}
        if user_input is not None:
            try:
                await _validate_host(self.hass, user_input["host"])
            except YallarmApiError:
                errors["base"] = "cannot_connect"
            else:
                self.hass.config_entries.async_update_entry(
                    self.config_entry,
                    data={**self.config_entry.data, "host": user_input["host"]},
                )
                return self.async_create_entry(title="", data={})
        current_host = self.config_entry.data.get("host", "")
        schema = vol.Schema({vol.Required("host", default=current_host): str})
        return self.async_show_form(step_id="init", data_schema=schema, errors=errors)
```

- [ ] **Step 2: Syntax check**

Run: `cd ~/code/yallarm && python3 -c "import ast; ast.parse(open('custom_components/yallarm/config_flow.py').read()); print('config_flow.py ok')"`
Expected: `config_flow.py ok`, no errors. (The setup form and options form are exercised live in Task 11/12 — a config flow needs a running `hass` to drive.)

- [ ] **Step 3: Commit**

```bash
cd ~/code/yallarm
git add custom_components/yallarm/config_flow.py
git commit -m "Add yallarm config + options flow"
```

---

### Task 6: Integration entry point (`__init__.py`)

**Files:**
- Create: `custom_components/yallarm/__init__.py`

**Interfaces:**
- Consumes: `const.DOMAIN`, `coordinator.YallarmCoordinator`.
- Produces: `async_setup_entry`, `async_unload_entry` (the two functions Home Assistant calls by convention when a config entry is added/removed). Stores the live `YallarmCoordinator` at `hass.data[DOMAIN][entry.entry_id]` — every platform's `async_setup_entry` (Tasks 7-9) reads it from there.

- [ ] **Step 1: Write `custom_components/yallarm/__init__.py`**

```python
"""The Yall-ARM integration."""
from __future__ import annotations

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant

from .const import DOMAIN
from .coordinator import YallarmCoordinator

PLATFORMS = ["sensor", "binary_sensor", "switch", "number", "button"]


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    coordinator = YallarmCoordinator(hass, entry.data["host"])
    await coordinator.async_config_entry_first_refresh()

    hass.data.setdefault(DOMAIN, {})[entry.entry_id] = coordinator

    entry.async_on_unload(entry.add_update_listener(_async_update_listener))

    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    return True


async def _async_update_listener(hass: HomeAssistant, entry: ConfigEntry) -> None:
    """Re-point the coordinator at the new host when the options flow changes it."""
    coordinator: YallarmCoordinator = hass.data[DOMAIN][entry.entry_id]
    coordinator.host = entry.data["host"]
    await coordinator.async_request_refresh()


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    unloaded = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unloaded:
        hass.data[DOMAIN].pop(entry.entry_id)
    return unloaded
```

- [ ] **Step 2: Syntax check**

Run: `cd ~/code/yallarm && python3 -c "import ast; ast.parse(open('custom_components/yallarm/__init__.py').read()); print('__init__.py ok')"`
Expected: `__init__.py ok`, no errors.

- [ ] **Step 3: Commit**

```bash
cd ~/code/yallarm
git add custom_components/yallarm/__init__.py
git commit -m "Add yallarm integration entry point (setup/unload)"
```

---

### Task 7: Read-only entities (`sensor.py`, `binary_sensor.py`)

**Files:**
- Create: `custom_components/yallarm/sensor.py`
- Create: `custom_components/yallarm/binary_sensor.py`

**Interfaces:**
- Consumes: `const.DOMAIN`, `coordinator.YallarmCoordinator`, `entity.YallarmEntity`.
- Produces: nothing consumed elsewhere — these are leaf platform modules, discovered by Home Assistant via the `PLATFORMS` list in `__init__.py`.

- [ ] **Step 1: Write `custom_components/yallarm/sensor.py`**

```python
"""Read-only sensors: WIS score/percent/forecast/threshold/mode, LED state."""
from __future__ import annotations

from homeassistant.components.sensor import SensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN
from .coordinator import YallarmCoordinator
from .entity import YallarmEntity

SENSORS: tuple[tuple[str, str, str | None], ...] = (
    ("wis_score", "WIS Score", None),
    ("wis_pct", "WIS Percent", "%"),
    ("score_30m", "WIS 30m Forecast", None),
    ("threshold", "Threshold", None),
    ("mode", "Mode", None),
    ("state", "LED State", None),
)


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    coordinator: YallarmCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities(
        YallarmSensor(coordinator, entry.entry_id, key, name, unit)
        for key, name, unit in SENSORS
    )


class YallarmSensor(YallarmEntity, SensorEntity):
    def __init__(
        self,
        coordinator: YallarmCoordinator,
        entry_id: str,
        key: str,
        name: str,
        unit: str | None,
    ) -> None:
        super().__init__(coordinator, entry_id)
        self._key = key
        self._attr_name = name
        self._attr_unique_id = f"{entry_id}_{key}"
        self._attr_native_unit_of_measurement = unit

    @property
    def native_value(self):
        return self.coordinator.data.get(self._key)
```

- [ ] **Step 2: Write `custom_components/yallarm/binary_sensor.py`**

```python
"""Read-only binary sensor: on-air status."""
from __future__ import annotations

from homeassistant.components.binary_sensor import BinarySensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN
from .coordinator import YallarmCoordinator
from .entity import YallarmEntity


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    coordinator: YallarmCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities([YallarmLiveBinarySensor(coordinator, entry.entry_id)])


class YallarmLiveBinarySensor(YallarmEntity, BinarySensorEntity):
    _attr_name = "Live"

    def __init__(self, coordinator: YallarmCoordinator, entry_id: str) -> None:
        super().__init__(coordinator, entry_id)
        self._attr_unique_id = f"{entry_id}_is_live"

    @property
    def is_on(self) -> bool:
        return bool(self.coordinator.data.get("is_live"))
```

- [ ] **Step 3: Syntax check both files**

Run:
```bash
cd ~/code/yallarm
python3 -c "import ast; ast.parse(open('custom_components/yallarm/sensor.py').read()); print('sensor.py ok')"
python3 -c "import ast; ast.parse(open('custom_components/yallarm/binary_sensor.py').read()); print('binary_sensor.py ok')"
```
Expected: both print `ok`, no errors.

- [ ] **Step 4: Commit**

```bash
cd ~/code/yallarm
git add custom_components/yallarm/sensor.py custom_components/yallarm/binary_sensor.py
git commit -m "Add yallarm read-only sensor and binary_sensor entities"
```

---

### Task 8: Switches (`switch.py`)

**Files:**
- Create: `custom_components/yallarm/switch.py`

**Interfaces:**
- Consumes: `api.async_post`, `const.DOMAIN`, `coordinator.YallarmCoordinator`, `entity.YallarmEntity`.
- Produces: nothing consumed elsewhere.

- [ ] **Step 1: Write `custom_components/yallarm/switch.py`**

```python
"""Switches: Dark Mode, Power, Logo Override."""
from __future__ import annotations

from homeassistant.components.switch import SwitchEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .api import async_post
from .const import DOMAIN
from .coordinator import YallarmCoordinator
from .entity import YallarmEntity

SWITCHES: tuple[tuple[str, str, str, str], ...] = (
    ("dark_mode", "Dark Mode", "/dark-mode-on", "/dark-mode-off"),
    ("power_on", "Power", "/power-on", "/power-off"),
    ("logo_override_on", "Logo Override", "/logo-on", "/logo-off"),
)


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    coordinator: YallarmCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities(
        YallarmSwitch(coordinator, entry.entry_id, key, name, on_path, off_path)
        for key, name, on_path, off_path in SWITCHES
    )


class YallarmSwitch(YallarmEntity, SwitchEntity):
    def __init__(
        self,
        coordinator: YallarmCoordinator,
        entry_id: str,
        key: str,
        name: str,
        on_path: str,
        off_path: str,
    ) -> None:
        super().__init__(coordinator, entry_id)
        self._key = key
        self._on_path = on_path
        self._off_path = off_path
        self._attr_name = name
        self._attr_unique_id = f"{entry_id}_{key}"

    @property
    def is_on(self) -> bool:
        return bool(self.coordinator.data.get(self._key))

    async def async_turn_on(self, **kwargs) -> None:
        session = async_get_clientsession(self.hass)
        await async_post(session, self.coordinator.host, self._on_path)
        await self.coordinator.async_request_refresh()

    async def async_turn_off(self, **kwargs) -> None:
        session = async_get_clientsession(self.hass)
        await async_post(session, self.coordinator.host, self._off_path)
        await self.coordinator.async_request_refresh()
```

- [ ] **Step 2: Syntax check**

Run: `cd ~/code/yallarm && python3 -c "import ast; ast.parse(open('custom_components/yallarm/switch.py').read()); print('switch.py ok')"`
Expected: `switch.py ok`, no errors.

- [ ] **Step 3: Commit**

```bash
cd ~/code/yallarm
git add custom_components/yallarm/switch.py
git commit -m "Add yallarm switch entities (dark mode, power, logo override)"
```

---

### Task 9: Numbers and buttons (`number.py`, `button.py`)

**Files:**
- Create: `custom_components/yallarm/number.py`
- Create: `custom_components/yallarm/button.py`

**Interfaces:**
- Consumes: `api.async_post`, `const.DOMAIN`, `coordinator.YallarmCoordinator`, `entity.YallarmEntity`.
- Produces: nothing consumed elsewhere.

- [ ] **Step 1: Write `custom_components/yallarm/number.py`**

```python
"""Numbers: Opacity (0-100), Bar Override (1-100, always activates the override)."""
from __future__ import annotations

from homeassistant.components.number import NumberEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .api import async_post
from .const import DOMAIN
from .coordinator import YallarmCoordinator
from .entity import YallarmEntity

NUMBERS: tuple[tuple[str, str, str, float, float], ...] = (
    ("opacity", "Opacity", "/opacity", 0, 100),
    ("bar_override_pct", "Bar Override", "/override", 1, 100),
)


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    coordinator: YallarmCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities(
        YallarmNumber(coordinator, entry.entry_id, key, name, path, min_value, max_value)
        for key, name, path, min_value, max_value in NUMBERS
    )


class YallarmNumber(YallarmEntity, NumberEntity):
    _attr_native_step = 1

    def __init__(
        self,
        coordinator: YallarmCoordinator,
        entry_id: str,
        key: str,
        name: str,
        path: str,
        min_value: float,
        max_value: float,
    ) -> None:
        super().__init__(coordinator, entry_id)
        self._key = key
        self._path = path
        self._attr_name = name
        self._attr_unique_id = f"{entry_id}_{key}"
        self._attr_native_min_value = min_value
        self._attr_native_max_value = max_value
        self._attr_native_unit_of_measurement = "%"

    @property
    def native_value(self) -> float | None:
        return self.coordinator.data.get(self._key)

    async def async_set_native_value(self, value: float) -> None:
        session = async_get_clientsession(self.hass)
        await async_post(session, self.coordinator.host, self._path, {"level": int(value)})
        await self.coordinator.async_request_refresh()
```

- [ ] **Step 2: Write `custom_components/yallarm/button.py`**

```python
"""Buttons: Reset, Test Audio, Test Audio Stop."""
from __future__ import annotations

from homeassistant.components.button import ButtonEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .api import async_post
from .const import DOMAIN
from .coordinator import YallarmCoordinator
from .entity import YallarmEntity

BUTTONS: tuple[tuple[str, str, str], ...] = (
    ("reset", "Reset", "/reset"),
    ("test_audio", "Test Audio", "/test-audio"),
    ("test_audio_stop", "Test Audio Stop", "/test-audio-stop"),
)


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    coordinator: YallarmCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities(
        YallarmButton(coordinator, entry.entry_id, key, name, path)
        for key, name, path in BUTTONS
    )


class YallarmButton(YallarmEntity, ButtonEntity):
    def __init__(
        self,
        coordinator: YallarmCoordinator,
        entry_id: str,
        key: str,
        name: str,
        path: str,
    ) -> None:
        super().__init__(coordinator, entry_id)
        self._path = path
        self._attr_name = name
        self._attr_unique_id = f"{entry_id}_{key}"

    async def async_press(self) -> None:
        session = async_get_clientsession(self.hass)
        await async_post(session, self.coordinator.host, self._path)
        await self.coordinator.async_request_refresh()
```

- [ ] **Step 3: Syntax check both files**

Run:
```bash
cd ~/code/yallarm
python3 -c "import ast; ast.parse(open('custom_components/yallarm/number.py').read()); print('number.py ok')"
python3 -c "import ast; ast.parse(open('custom_components/yallarm/button.py').read()); print('button.py ok')"
```
Expected: both print `ok`, no errors.

- [ ] **Step 4: Commit**

```bash
cd ~/code/yallarm
git add custom_components/yallarm/number.py custom_components/yallarm/button.py
git commit -m "Add yallarm number and button entities"
```

---

### Task 10: Push and install via HACS

**Files:** none (deployment step).

**Interfaces:**
- Consumes: nothing new — this task installs everything built in Tasks 1-9 onto the live Home Assistant instance.

- [ ] **Step 1: Push all commits so far**

```bash
cd ~/code/yallarm
git push origin main
```
Expected: push succeeds, no conflicts (everything so far has been committed incrementally per task).

- [ ] **Step 2: Register the yallarm repo as a HACS custom repository**

Call the Home Assistant MCP tool:
```
ha_manage_hacs(action="add_repository", repository="ericbram/yallarm", category="integration")
```
Expected: success response confirming the repository is registered.

- [ ] **Step 3: Install it**

```
ha_manage_hacs(action="download", repository_id="ericbram/yallarm")
```
Expected: success response. Note the caveat from the tool's own docs: installing an integration usually needs a Home Assistant restart to activate.

- [ ] **Step 4: Restart Home Assistant**

Call `ha_restart(confirm=True)` (or use `ha_get_system_health(include="config_check")` first if available, per that tool's best-practice note). Wait for HA to come back — poll `ha_get_overview(fields=["system_info"])` every ~15s until `system_info.state` is `"RUNNING"` again, up to ~5 minutes.

- [ ] **Step 5: Confirm the integration is installed**

```
ha_get_integration(query="yallarm")
```
Expected: no entry yet (installing ≠ configuring) — this call just confirms the domain loaded without a `setup_error`/`migration_error` state if HA already tried to discover it; more likely it returns empty until Task 11 adds a config entry. If it returns an entry in `setup_error` state, stop and read the error before proceeding — that means a bug in Tasks 1-9 that a syntax check couldn't catch (e.g. a bad import).

---

### Task 11: Add the integration and verify entities live

**Files:** none.

**Interfaces:**
- Consumes: the installed integration from Task 10.

- [ ] **Step 1: Add the config entry**

```
ha_set_integration(domain="yallarm", config={"host": "192.168.0.117"})
```
Expected: success, driving through `async_step_user` — confirms `config_flow.py`'s validation call against the real device succeeded.

- [ ] **Step 2: Confirm all 12 entities exist with sane values**

```
ha_search(domain_filter="sensor", query="yall")
ha_search(domain_filter="binary_sensor", query="yall")
ha_search(domain_filter="switch", query="yall")
ha_search(domain_filter="number", query="yall")
ha_search(domain_filter="button", query="yall")
```
Expected: 6 sensors (WIS Score, WIS Percent, WIS 30m Forecast, Threshold, Mode, LED State), 1 binary_sensor (Live), 3 switches (Dark Mode, Power, Logo Override), 2 numbers (Opacity, Bar Override), 3 buttons (Reset, Test Audio, Test Audio Stop) — 15 entities total, all under one "Yall-ARM" device, with sensor/binary_sensor states matching a fresh `curl http://192.168.0.117/status` taken at the same time.

- [ ] **Step 3: Exercise a switch and a number**

Call `ha_call_service` (or the equivalent write path) to turn on `switch.yall_arm_dark_mode`, then re-fetch `sensor` state and confirm `opacity` dropped to 25 (matching the firmware's dark-mode preset). Set `number.yall_arm_opacity` to `60`, then curl `http://192.168.0.117/status` directly and confirm `"opacity":60`. Turn `switch.yall_arm_power` on afterward to restore `opacity` to 100 and clear dark mode, matching the firmware's power-on preset (same behavior verified manually in the previous session).

- [ ] **Step 4: Exercise a button**

Press `button.yall_arm_reset`, confirm no error and that `bar_override`/`logo_override` read back `false` from a follow-up `/status` curl.

---

### Task 12: Verify the options flow (host-change recovery)

**Files:** none.

**Interfaces:**
- Consumes: the config entry from Task 11.

- [ ] **Step 1: Point the entry at an unreachable IP**

Use `ha_set_integration(entry_id="<the yallarm entry_id from Task 11>", config={"host": "192.168.0.254"})` — this drives the options flow's `async_step_init`. Since `192.168.0.254` is unused on this network, expect the validation call to fail and the options flow to return the `cannot_connect` error rather than accepting the change — confirming `config_flow.py`'s validation is actually enforced, not just cosmetic.

- [ ] **Step 2: Confirm entities remain on the last-known-good host**

Re-check `sensor.yall_arm_wis_score`'s state — it should still be updating from `192.168.0.117`, proving the rejected options-flow submission didn't corrupt the live config entry.

- [ ] **Step 3: Confirm a real host change works**

The device's IP is expected to stay `192.168.0.117` for this test since it hasn't been reflashed since Task 10-11. Round-trip the *same* value through the options flow anyway: `ha_set_integration(entry_id="<entry_id>", config={"host": "192.168.0.117"})`, confirm it's accepted (no error), and that `coordinator.host` updates without requiring `async_unload_entry`/`async_setup_entry` to run again — i.e. entities keep updating on the next poll with no gap in state history.

- [ ] **Step 4: Verify unavailable/recovery on a real coordinator failure (manual, requires the user)**

Step 1 tested the config-flow validation path (a bad host is rejected before it ever reaches the coordinator) — it does not exercise `coordinator.py`'s `_async_update_data` → `UpdateFailed` path, since that only fires once a *previously-valid* host stops responding. That requires the device to actually go offline, which can't be done from here — ask the user to briefly power off the yallarm device. Then:
- Wait ~60s (two poll cycles) and re-check `sensor.yall_arm_wis_score` — expect its state to become `unavailable`, along with every other yallarm entity.
- Ask the user to power the device back on, wait ~30s, and re-check — expect all entities to recover to live values with no further action needed (no re-add, no restart).

---

### Task 13: Tear down the previously-staged YAML approach and rebuild the dashboard

**Files:**
- Delete: `homeassistant/yallarm.yaml`

**Interfaces:** none — this is cleanup, not new code.

- [ ] **Step 1: Delete the old reference YAML and its containing folder**

```bash
cd ~/code/yallarm
git rm homeassistant/yallarm.yaml
rmdir homeassistant 2>/dev/null || true
```
Note: this YAML was never actually pasted into the live `configuration.yaml` (confirmed in the prior session — the user hadn't done that step before the HACS approach was chosen instead), so there is no `configuration.yaml` cleanup needed on the HA side for the `rest`/`rest_command` blocks themselves.

- [ ] **Step 2: Remove the now-superseded helper**

The `input_text.yall_arm_host` helper is superseded by the config entry's options flow. Find and remove it:
```
ha_search(domain_filter="input_text", query="yall_arm_host")
```
then remove it via `ha_remove_helpers_integrations` (or the equivalent removal tool) using the entity_id/helper_id found above.

- [ ] **Step 3: Remove the now-superseded automation**

The `automation.yall_arm_push_opacity_to_device` automation is superseded by `number.yall_arm_opacity`'s direct write. Remove it via the automation-removal tool using identifier `automation.yall_arm_push_opacity_to_device`.

- [ ] **Step 4: Rebuild the dashboard section against the new native entities**

Fetch the current `home-landing` dashboard config (`ha_config_get_dashboard(url_path="home-landing")`) to get a fresh `config_hash`, then read the best-practices skill for a fresh `BestPracticeKey` (`ha_get_skill_guide(skill="home-assistant-best-practices", file="SKILL.md")`), then replace the previously-added "Y'all-ARM" grid section (search it via `ha_config_get_dashboard(heading="Y'all-ARM")` to get its exact `python_path`) with one referencing the new entities:

```python
config['views'][0]['sections'][<index found above>] = {
    'type': 'grid',
    'cards': [
        {'type': 'heading', 'heading': "Y'all-ARM", 'icon': 'mdi:weather-lightning'},
        {'type': 'horizontal-stack', 'cards': [
            {'type': 'tile', 'entity': 'binary_sensor.yall_arm_live', 'name': 'Status'},
            {'type': 'tile', 'entity': 'sensor.yall_arm_wis_percent', 'name': 'WIS'},
        ]},
        {'type': 'tile', 'entity': 'number.yall_arm_opacity', 'name': 'Opacity',
         'features': [{'type': 'numeric-input', 'style': 'slider'}]},
        {'type': 'horizontal-stack', 'cards': [
            {'type': 'tile', 'entity': 'switch.yall_arm_dark_mode', 'name': 'Dark Mode'},
            {'type': 'tile', 'entity': 'switch.yall_arm_power', 'name': 'Power'},
            {'type': 'tile', 'entity': 'switch.yall_arm_logo_override', 'name': 'Logo Override'},
        ]},
    ],
}
```
(Confirm the exact entity_ids HA actually assigned in Task 11's `ha_search` results first — they should match this naming from the `has_entity_name` + device-name pattern, but slugging is HA's to decide, not ours to assume blindly.) Apply via `ha_config_set_dashboard(url_path="home-landing", config_hash=<fresh hash>, python_transform=<above>, BestPracticeKey=<fresh key>)`.

- [ ] **Step 5: Commit the teardown**

```bash
cd ~/code/yallarm
git add -A
git commit -m "Remove staged rest/rest_command YAML approach, superseded by the HACS integration"
git push origin main
```

---

### Task 14: Final review

**Files:** none.

- [ ] **Step 1: Confirm the full file tree matches the spec**

```bash
cd ~/code/yallarm
find custom_components/yallarm hacs.json -type f | sort
```
Expected: `hacs.json`, and inside `custom_components/yallarm/`: `__init__.py`, `api.py`, `binary_sensor.py`, `button.py`, `config_flow.py`, `const.py`, `coordinator.py`, `entity.py`, `manifest.json`, `number.py`, `sensor.py`, `strings.json`, `switch.py` — 13 files total, matching the spec's repo layout section exactly.

- [ ] **Step 2: Confirm `git log` shows the full task history and the working tree is clean**

```bash
cd ~/code/yallarm
git status --porcelain
git log --oneline -20
```
Expected: empty `git status --porcelain` output (everything committed and pushed), and a log entry per task above.
