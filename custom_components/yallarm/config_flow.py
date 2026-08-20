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
