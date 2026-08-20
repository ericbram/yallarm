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
