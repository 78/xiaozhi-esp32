"""
WebSocket upstream client: connects to the official XiaoZhi server on behalf of the device.
Handles hello handshake translation and bidirectional message/audio forwarding.
"""

import json
import struct
import asyncio
import logging
import websockets
from websockets.protocol import State as WsState

log = logging.getLogger(__name__)


class WsUpstream:
    def __init__(self, cfg: dict, device_state):
        self.cfg = cfg
        self.state = device_state
        self.ws = None
        self._on_json = None
        self._on_audio = None
        self._recv_task = None

    def on_json(self, callback):
        """Register callback for JSON messages from official server."""
        self._on_json = callback

    def on_audio(self, callback):
        """Register callback for binary audio from official server."""
        self._on_audio = callback

    async def connect(self, device_hello: dict) -> dict | None:
        """
        Open WebSocket to official server, perform hello handshake.
        Returns the translated server hello (with transport=udp and udp config)
        or None on failure.
        """
        creds = self.state.credentials
        if not creds.ws_url or not creds.ws_token:
            log.error("No upstream WS credentials available")
            return None

        token = creds.ws_token
        if " " not in token:
            token = f"Bearer {token}"

        headers = {
            "Authorization": token,
            "Protocol-Version": str(device_hello.get("version", 3)),
            "Device-Id": creds.device_id,
            "Client-Id": creds.client_id,
        }

        try:
            self.ws = await websockets.connect(
                creds.ws_url,
                additional_headers=headers,
                ping_interval=30,
                ping_timeout=10,
            )
        except Exception as e:
            log.error("Failed to connect upstream WS: %s", e)
            return None

        upstream_hello = dict(device_hello)
        upstream_hello["transport"] = "websocket"
        await self.ws.send(json.dumps(upstream_hello))
        log.info("Sent hello to upstream (transport=websocket)")

        try:
            raw = await asyncio.wait_for(self.ws.recv(), timeout=10)
        except Exception as e:
            log.error("Timeout waiting for upstream hello: %s", e)
            await self.close()
            return None

        try:
            server_hello = json.loads(raw)
        except json.JSONDecodeError:
            log.error("Invalid JSON in upstream hello")
            await self.close()
            return None

        if server_hello.get("type") != "hello":
            log.error("Unexpected upstream message type: %s", server_hello.get("type"))
            await self.close()
            return None

        log.info("Upstream hello received: session_id=%s", server_hello.get("session_id"))

        session = self.state.new_audio_session()
        session.session_id = server_hello.get("session_id", "")

        translated = dict(server_hello)
        translated["transport"] = "udp"
        translated["udp"] = {
            "server": self.cfg["proxy"]["public_host"],
            "port": self.cfg["udp"]["port"],
            "key": session.aes_key.hex().upper(),
            "nonce": session.aes_nonce.hex().upper(),
        }

        self._recv_task = asyncio.create_task(self._receive_loop())
        self.state.ws_connected = True
        return translated

    async def _receive_loop(self):
        try:
            async for msg in self.ws:
                if isinstance(msg, bytes):
                    self._handle_binary(msg)
                else:
                    self._handle_text(msg)
        except websockets.exceptions.ConnectionClosed as e:
            log.info("Upstream WS closed: %s", e)
        except Exception as e:
            log.error("Upstream WS error: %s", e)
        finally:
            self.state.ws_connected = False
            self.state.close_audio_session()
            if self._on_json:
                goodbye = {"type": "goodbye", "session_id": self.state.audio_session.session_id}
                self._on_json(goodbye)

    def _handle_text(self, raw: str):
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            log.error("Invalid JSON from upstream: %s", raw[:200])
            return

        msg_type = data.get("type", "")
        log.debug("Upstream JSON: type=%s", msg_type)

        if self._on_json:
            self._on_json(data)

    def _handle_binary(self, data: bytes):
        """Parse upstream binary frame and extract Opus payload."""
        if len(data) < 4:
            return

        version = self.state.audio_session.session_id  # version from hello
        # Try BinaryProtocol3 (most common with version=3): type(1) reserved(1) payload_size(2) payload
        bp3_type = data[0]
        bp3_payload_size = struct.unpack(">H", data[2:4])[0]

        if bp3_payload_size > 0 and 4 + bp3_payload_size <= len(data):
            opus_data = data[4:4 + bp3_payload_size]
            timestamp = 0
            if self._on_audio:
                self._on_audio(opus_data, timestamp)
            return

        # Fallback: raw Opus (version 1)
        if self._on_audio:
            self._on_audio(data, 0)

    def _ws_open(self) -> bool:
        return self.ws is not None and self.ws.state == WsState.OPEN

    async def send_text(self, text: str):
        if self._ws_open():
            await self.ws.send(text)

    async def send_audio(self, opus_data: bytes, timestamp: int):
        """Send Opus audio to upstream server as WebSocket binary (BinaryProtocol3)."""
        if not self._ws_open():
            return
        header = struct.pack(">BBH", 0, 0, len(opus_data))
        await self.ws.send(header + opus_data)

    async def close(self):
        if self._recv_task:
            self._recv_task.cancel()
            self._recv_task = None
        if self.ws:
            try:
                await self.ws.close()
            except Exception:
                pass
            self.ws = None
        self.state.ws_connected = False
        log.info("Upstream WS closed")
