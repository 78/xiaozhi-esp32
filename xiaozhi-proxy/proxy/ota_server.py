"""
OTA HTTP Server: intercepts the device's OTA request, forwards to api.tenclass.net,
modifies the response to steer the device toward our MQTT broker.
"""

import json
import logging
from aiohttp import web, ClientSession

log = logging.getLogger(__name__)


class OtaServer:
    def __init__(self, cfg: dict, device_state):
        self.cfg = cfg
        self.state = device_state
        self.app = web.Application()
        self.app.router.add_route("*", "/xiaozhi/ota/{tail:.*}", self.handle_ota)
        self.app.router.add_route("*", "/xiaozhi/ota/", self.handle_ota)
        self.app.router.add_post("/xiaozhi/ota", self.handle_ota)

    async def handle_ota(self, request: web.Request) -> web.Response:
        log.info("OTA request from device: %s %s", request.method, request.path)

        self.state.store_ota_headers(dict(request.headers))

        body = await request.read()
        upstream_url = self.cfg["ota"]["upstream_url"]

        forward_headers = {}
        for key in ("Device-Id", "Client-Id", "Serial-Number", "User-Agent",
                     "Activation-Version", "Accept-Language", "Content-Type"):
            if key in request.headers:
                forward_headers[key] = request.headers[key]

        async with ClientSession() as session:
            method = request.method
            async with session.request(method, upstream_url,
                                       headers=forward_headers, data=body) as resp:
                upstream_status = resp.status
                upstream_body = await resp.text()
                log.info("Upstream OTA response: status=%d len=%d",
                         upstream_status, len(upstream_body))

        if upstream_status != 200:
            return web.Response(status=upstream_status, text=upstream_body)

        try:
            data = json.loads(upstream_body)
        except json.JSONDecodeError:
            log.error("Failed to parse upstream OTA JSON")
            return web.Response(status=502, text="Bad upstream response")

        ws_section = data.get("websocket")
        if isinstance(ws_section, dict):
            self.state.store_ws_config(
                ws_section.get("url", ""),
                ws_section.get("token", ""),
            )

        mqtt_section = data.get("mqtt")
        if isinstance(mqtt_section, dict):
            orig_client_id = mqtt_section.get("client_id", "")
        else:
            orig_client_id = f"xiaozhi_{self.state.credentials.device_id}"

        public_host = self.cfg["proxy"]["public_host"]
        mqtt_port = self.cfg["mqtt"]["broker_port"]
        data["mqtt"] = {
            "endpoint": f"{public_host}:{mqtt_port}",
            "client_id": orig_client_id,
            "username": self.cfg["mqtt"]["device_username"],
            "password": self.cfg["mqtt"]["device_password"],
            "publish_topic": self.cfg["mqtt"]["device_up_topic"],
            "subscribe_topic": self.cfg["mqtt"]["device_down_topic"],
            "keepalive": 240,
        }

        data.pop("websocket", None)

        modified = json.dumps(data, ensure_ascii=False)
        log.info("Modified OTA response: mqtt.endpoint=%s:%s", public_host, mqtt_port)
        return web.Response(
            status=200,
            text=modified,
            content_type="application/json",
        )

    async def start(self):
        port = self.cfg["ota"]["port"]
        runner = web.AppRunner(self.app)
        await runner.setup()
        site = web.TCPSite(runner, self.cfg["proxy"]["host"], port)
        await site.start()
        log.info("OTA server listening on port %d", port)
