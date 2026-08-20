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
