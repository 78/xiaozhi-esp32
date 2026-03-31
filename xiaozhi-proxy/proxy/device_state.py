"""
Central state store for a connected XiaoZhi device.
Holds credentials captured from OTA, current session info, and audio channel state.
Persists credentials to disk so they survive proxy restarts.
"""

import os
import json
import secrets
import logging
from dataclasses import dataclass, field

log = logging.getLogger(__name__)

CREDS_FILE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".device_creds.json")


@dataclass
class DeviceCredentials:
    """Credentials captured from the device's OTA request and response."""
    device_id: str = ""
    client_id: str = ""
    serial_number: str = ""
    user_agent: str = ""
    ws_url: str = ""
    ws_token: str = ""


@dataclass
class AudioSession:
    """State for one audio conversation session."""
    session_id: str = ""
    aes_key: bytes = field(default_factory=lambda: b"")
    aes_nonce: bytes = field(default_factory=lambda: b"")
    device_udp_addr: tuple = field(default_factory=lambda: ("", 0))
    local_sequence: int = 0
    remote_sequence: int = 0
    active: bool = False


class DeviceState:
    def __init__(self):
        self.credentials = DeviceCredentials()
        self.audio_session = AudioSession()
        self.mqtt_connected = False
        self.ws_connected = False
        self._load_credentials()

    def _load_credentials(self):
        try:
            with open(CREDS_FILE, "r") as f:
                data = json.load(f)
            self.credentials.device_id = data.get("device_id", "")
            self.credentials.client_id = data.get("client_id", "")
            self.credentials.ws_url = data.get("ws_url", "")
            self.credentials.ws_token = data.get("ws_token", "")
            if self.credentials.ws_url:
                log.info("Loaded persisted credentials: device=%s ws_url=%s",
                         self.credentials.device_id, self.credentials.ws_url)
        except (FileNotFoundError, json.JSONDecodeError):
            pass

    def _save_credentials(self):
        try:
            with open(CREDS_FILE, "w") as f:
                json.dump({
                    "device_id": self.credentials.device_id,
                    "client_id": self.credentials.client_id,
                    "ws_url": self.credentials.ws_url,
                    "ws_token": self.credentials.ws_token,
                }, f)
        except Exception as e:
            log.error("Failed to save credentials: %s", e)

    def store_ota_headers(self, headers: dict):
        self.credentials.device_id = headers.get("Device-Id", "")
        self.credentials.client_id = headers.get("Client-Id", "")
        self.credentials.serial_number = headers.get("Serial-Number", "")
        self.credentials.user_agent = headers.get("User-Agent", "")
        log.info("Stored device headers: id=%s client=%s",
                 self.credentials.device_id, self.credentials.client_id)
        self._save_credentials()

    def store_ws_config(self, url: str, token: str):
        self.credentials.ws_url = url
        self.credentials.ws_token = token
        log.info("Stored upstream WS config: url=%s", url)
        self._save_credentials()

    def new_audio_session(self) -> AudioSession:
        nonce = bytearray(secrets.token_bytes(16))
        nonce[0] = 0x01  # packet type = audio
        nonce[1] = 0x00  # flags = 0
        self.audio_session = AudioSession(
            aes_key=secrets.token_bytes(16),
            aes_nonce=bytes(nonce),
            active=True,
        )
        log.info("Created new audio session, key=%s", self.audio_session.aes_key.hex())
        return self.audio_session

    def close_audio_session(self):
        self.audio_session.active = False
        log.info("Audio session closed")
