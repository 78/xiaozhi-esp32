"""
MQTT Bridge: connects to the local Mosquitto broker, receives device messages,
routes them to the upstream WebSocket or OpenClaw, and publishes replies back.
"""

import json
import asyncio
import logging
import aiomqtt

from .ws_upstream import WsUpstream
from .udp_audio import UdpAudioServer
from .intent_router import detect_intent
from .openclaw_client import OpenClawClient

log = logging.getLogger(__name__)


class MqttBridge:
    def __init__(self, cfg: dict, device_state, udp_server: UdpAudioServer):
        self.cfg = cfg
        self.state = device_state
        self.udp_server = udp_server
        self.ws_upstream = WsUpstream(cfg, device_state)
        self.openclaw = OpenClawClient(cfg)
        self._client: aiomqtt.Client | None = None
        self._up_topic = cfg["mqtt"]["device_up_topic"]
        self._down_topic = cfg["mqtt"]["device_down_topic"]

    async def publish_to_device(self, payload: str):
        if self._client:
            await self._client.publish(self._down_topic, payload)

    async def _on_device_message(self, raw: str):
        """Handle a JSON message from the device (via MQTT)."""
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            log.error("Invalid JSON from device: %s", raw[:200])
            return

        msg_type = data.get("type", "")
        log.info("Device -> Proxy: type=%s", msg_type)

        if msg_type == "hello":
            await self._handle_device_hello(data)
        elif msg_type == "goodbye":
            await self._handle_device_goodbye(data)
        else:
            await self._forward_to_upstream(data)

    async def _handle_device_hello(self, hello: dict):
        """Device wants to open an audio channel. Connect upstream and translate."""
        log.info("Device hello received, connecting to upstream...")

        self.ws_upstream.on_json(self._on_upstream_json_sync)
        self.ws_upstream.on_audio(self._on_upstream_audio)

        self.udp_server.on_decrypted_audio(self._on_device_audio)

        translated_hello = await self.ws_upstream.connect(hello)
        if translated_hello is None:
            error = json.dumps({
                "type": "goodbye",
                "session_id": "",
            })
            await self.publish_to_device(error)
            return

        await self.publish_to_device(json.dumps(translated_hello, ensure_ascii=False))
        log.info("Sent translated hello to device (transport=udp)")

    async def _handle_device_goodbye(self, data: dict):
        log.info("Device goodbye, closing upstream")
        if self.ws_upstream._ws_open():
            await self.ws_upstream.send_text(json.dumps(data))
        await self.ws_upstream.close()

    async def _forward_to_upstream(self, data: dict):
        """Forward device JSON to upstream WebSocket."""
        if self.ws_upstream._ws_open():
            await self.ws_upstream.send_text(json.dumps(data, ensure_ascii=False))
        else:
            log.warning("Upstream WS not connected, dropping message: type=%s",
                        data.get("type"))

    def _on_upstream_json_sync(self, data: dict):
        """Called from the WS receive loop (sync context). Schedule async handling."""
        asyncio.get_event_loop().create_task(self._on_upstream_json(data))

    async def _on_upstream_json(self, data: dict):
        """Handle JSON from the upstream official server."""
        msg_type = data.get("type", "")
        log.info("Upstream -> Device: type=%s", msg_type)

        if msg_type == "stt":
            text = data.get("text", "")
            intent, _ = detect_intent(text)
            if intent:
                asyncio.create_task(self._handle_openclaw_intent(intent, text))

        if msg_type == "goodbye":
            self.state.close_audio_session()
            await self.ws_upstream.close()

        await self.publish_to_device(json.dumps(data, ensure_ascii=False))

    async def _handle_openclaw_intent(self, intent: str, text: str):
        """Route detected intent to OpenClaw in background."""
        log.info("Routing to OpenClaw: intent=%s text=%s", intent, text[:60])
        try:
            if intent == "web_search":
                result = await self.openclaw.web_search(text)
            elif intent == "send_to_wechat":
                result = await self.openclaw.send_to_wechat(text)
            elif intent == "set_reminder":
                result = await self.openclaw.set_reminder("", text)
            else:
                result = await self.openclaw.chat(text)
            log.info("OpenClaw result: %s", result[:100])
        except Exception as e:
            log.error("OpenClaw call failed: %s", e)

    def _on_upstream_audio(self, opus_data: bytes, timestamp: int):
        """Forward upstream Opus audio to device via encrypted UDP."""
        self.udp_server.send_to_device(opus_data, timestamp)

    def _on_device_audio(self, opus_data: bytes, timestamp: int):
        """Forward device Opus audio to upstream via WebSocket."""
        if self.ws_upstream._ws_open():
            asyncio.get_event_loop().create_task(
                self.ws_upstream.send_audio(opus_data, timestamp)
            )

    async def start(self):
        mqtt_cfg = self.cfg["mqtt"]
        while True:
            try:
                async with aiomqtt.Client(
                    hostname=mqtt_cfg["broker_host"],
                    port=mqtt_cfg["broker_port"],
                    username=mqtt_cfg["bridge_username"],
                    password=mqtt_cfg["bridge_password"],
                    identifier="proxy_bridge",
                ) as client:
                    self._client = client
                    self.state.mqtt_connected = True
                    log.info("MQTT bridge connected to broker")

                    await client.subscribe(self._up_topic)
                    log.info("Subscribed to %s", self._up_topic)

                    async for message in client.messages:
                        raw = message.payload.decode("utf-8", errors="replace")
                        await self._on_device_message(raw)

            except aiomqtt.MqttError as e:
                log.error("MQTT connection error: %s", e)
            except Exception as e:
                log.error("MQTT bridge error: %s", e)
            finally:
                self._client = None
                self.state.mqtt_connected = False

            log.info("MQTT reconnecting in 5s...")
            await asyncio.sleep(5)
