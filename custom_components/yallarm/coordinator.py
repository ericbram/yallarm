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
