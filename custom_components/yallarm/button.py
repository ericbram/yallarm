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
