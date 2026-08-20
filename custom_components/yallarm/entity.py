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
