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
