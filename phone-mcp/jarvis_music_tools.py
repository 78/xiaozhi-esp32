"""Connects the offline music tools (local_music.py) to the phone MCP server.

server.py calls register(self) from inside Tools.__init__ (install_music.py adds that call).
Each music tool becomes an MCP tool; the blocking work (scanning the folder, starting the
player) runs in a thread so the connection to XiaoZhi stays responsive.
"""

import asyncio

from local_music import TOOLS, LocalMusic


def _handler(music, name):
    async def handler(args):
        try:
            return await asyncio.to_thread(music.call, name, args)
        except (TypeError, ValueError):
            return "I did not understand one of the values. Please try again."

    return handler


def register(tools):
    """Adds every local_music tool to the server's Tools object (via its _add method)."""
    music = LocalMusic()
    for name, description, properties, required in TOOLS:
        tools._add(name, description, properties, required, _handler(music, name))
    return music
