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
