"""
Push Manager: handles timed reminders and webhook callbacks from OpenClaw.
Pushes alert messages to the device via MQTT.
"""

import json
import logging
from aiohttp import web

log = logging.getLogger(__name__)


class PushManager:
    def __init__(self, cfg: dict, publish_fn):
        """
        publish_fn: async callable(json_str) that publishes to the device MQTT topic.
        """
        self.cfg = cfg
        self._publish = publish_fn
        self.app = web.Application()
        self.app.router.add_post("/webhook/reminder", self.handle_reminder_webhook)
        self.app.router.add_post("/push/alert", self.handle_push_alert)

    async def push_alert(self, status: str, message: str, emotion: str = "happy"):
        """Send an alert message to the device via MQTT."""
        payload = json.dumps({
            "type": "alert",
            "status": status,
            "message": message,
            "emotion": emotion,
        }, ensure_ascii=False)
        await self._publish(payload)
        log.info("Pushed alert: [%s] %s", status, message[:60])

    async def push_server_push(self):
        """
        Send a server_push message to trigger the device to open an audio channel.
        Requires the optional firmware modification.
        """
        payload = json.dumps({"type": "server_push"})
        await self._publish(payload)
        log.info("Pushed server_push trigger")

    async def handle_reminder_webhook(self, request: web.Request) -> web.Response:
        """
        Webhook endpoint for OpenClaw cron callbacks.
        Expected body: {"message": "reminder text", "time": "original time string"}
        """
        try:
            data = await request.json()
        except Exception:
            return web.Response(status=400, text="Invalid JSON")

        message = data.get("message", "你有一条提醒")
        await self.push_alert("提醒", message, "happy")
        return web.json_response({"status": "ok"})

    async def handle_push_alert(self, request: web.Request) -> web.Response:
        """
        Generic push endpoint for external services.
        Body: {"status": "...", "message": "...", "emotion": "..."}
        """
        try:
            data = await request.json()
        except Exception:
            return web.Response(status=400, text="Invalid JSON")

        status = data.get("status", "通知")
        message = data.get("message", "")
        emotion = data.get("emotion", "neutral")
        if not message:
            return web.Response(status=400, text="Missing message")

        await self.push_alert(status, message, emotion)
        return web.json_response({"status": "ok"})

    async def start(self):
        port = self.cfg["push"]["webhook_port"]
        runner = web.AppRunner(self.app)
        await runner.setup()
        site = web.TCPSite(runner, self.cfg["proxy"]["host"], port)
        await site.start()
        log.info("Push webhook server listening on port %d", port)
