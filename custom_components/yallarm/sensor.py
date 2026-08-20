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
