"""
XiaoZhi Transparent Proxy - Main entry point.
Starts all services: OTA server, MQTT bridge, UDP audio, push manager.
"""

import asyncio
import logging
import yaml
import sys
import os
import platform

if platform.system() == "Windows":
    asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from proxy.device_state import DeviceState
from proxy.ota_server import OtaServer
from proxy.mqtt_bridge import MqttBridge
from proxy.udp_audio import UdpAudioServer
from proxy.push_manager import PushManager

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)-14s] %(levelname)-5s %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("main")


def load_config(path: str = "config.yaml") -> dict:
    script_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    full_path = os.path.join(script_dir, path)
    with open(full_path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


async def main():
    cfg = load_config()
    log.info("XiaoZhi Transparent Proxy starting...")
    log.info("  OTA port:     %d", cfg["ota"]["port"])
    log.info("  MQTT broker:  %s:%d", cfg["mqtt"]["broker_host"], cfg["mqtt"]["broker_port"])
    log.info("  UDP port:     %d", cfg["udp"]["port"])
    log.info("  Push port:    %d", cfg["push"]["webhook_port"])
    log.info("  Public host:  %s", cfg["proxy"]["public_host"])

    state = DeviceState()

    ota = OtaServer(cfg, state)
    udp = UdpAudioServer(cfg, state)
    bridge = MqttBridge(cfg, state, udp)
    push = PushManager(cfg, publish_fn=bridge.publish_to_device)

    await ota.start()
    await udp.start()
    await push.start()

    log.info("All services started. Waiting for device connection...")
    await bridge.start()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        log.info("Shutting down")
