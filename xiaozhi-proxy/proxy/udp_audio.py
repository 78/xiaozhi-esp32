"""
UDP Audio Server: handles AES-CTR encrypted Opus audio between the device and proxy.
Decrypts device audio for forwarding upstream, encrypts server audio for the device.
"""

import struct
import asyncio
import logging
from Crypto.Cipher import AES

log = logging.getLogger(__name__)

NONCE_LEN = 16


def _build_nonce(base_nonce: bytes, payload_len: int, timestamp: int, sequence: int) -> bytes:
    """
    Construct the CTR nonce per xiaozhi convention:
      nonce[0:2]   = base nonce bytes
      nonce[2:4]   = payload_len  (big-endian uint16)
      nonce[4:8]   = base nonce bytes
      nonce[8:12]  = timestamp    (big-endian uint32)
      nonce[12:16] = sequence     (big-endian uint32)
    """
    n = bytearray(base_nonce)
    struct.pack_into(">H", n, 2, payload_len)
    struct.pack_into(">I", n, 8, timestamp)
    struct.pack_into(">I", n, 12, sequence)
    return bytes(n)


def aes_ctr_crypt(key: bytes, nonce: bytes, data: bytes) -> bytes:
    cipher = AES.new(key, AES.MODE_CTR, nonce=b"", initial_value=nonce)
    return cipher.encrypt(data)


class UdpAudioProtocol(asyncio.DatagramProtocol):
    def __init__(self, device_state, on_decrypted_audio):
        self.state = device_state
        self.on_decrypted_audio = on_decrypted_audio
        self.transport = None

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data: bytes, addr):
        session = self.state.audio_session
        if not session.active:
            return

        if session.device_udp_addr == ("", 0):
            session.device_udp_addr = addr
            log.info("Device UDP address learned: %s", addr)

        if len(data) < NONCE_LEN:
            log.warning("UDP packet too short: %d bytes", len(data))
            return

        # Parse header: type(1) flags(1) payload_len(2) ssrc(4) timestamp(4) sequence(4)
        pkt_type = data[0]
        if pkt_type != 0x01:
            log.warning("Unexpected UDP packet type: 0x%02x", pkt_type)
            return

        payload_len = struct.unpack(">H", data[2:4])[0]
        timestamp = struct.unpack(">I", data[8:12])[0]
        sequence = struct.unpack(">I", data[12:16])[0]
        encrypted_payload = data[NONCE_LEN:]

        nonce = _build_nonce(session.aes_nonce, len(encrypted_payload), timestamp, sequence)
        try:
            opus_data = aes_ctr_crypt(session.aes_key, nonce, encrypted_payload)
        except Exception as e:
            log.error("AES decrypt failed: %s", e)
            return

        session.remote_sequence = sequence

        if self.on_decrypted_audio:
            self.on_decrypted_audio(opus_data, timestamp)

    def send_to_device(self, opus_data: bytes, timestamp: int):
        session = self.state.audio_session
        if not session.active or session.device_udp_addr == ("", 0):
            return

        session.local_sequence += 1
        seq = session.local_sequence

        nonce = _build_nonce(session.aes_nonce, len(opus_data), timestamp, seq)
        encrypted = aes_ctr_crypt(session.aes_key, nonce, opus_data)
        packet = nonce + encrypted

        self.transport.sendto(packet, session.device_udp_addr)


class UdpAudioServer:
    def __init__(self, cfg: dict, device_state):
        self.cfg = cfg
        self.state = device_state
        self.protocol: UdpAudioProtocol | None = None
        self._on_decrypted_audio = None

    def on_decrypted_audio(self, callback):
        self._on_decrypted_audio = callback
        if self.protocol:
            self.protocol.on_decrypted_audio = callback

    def send_to_device(self, opus_data: bytes, timestamp: int):
        if self.protocol:
            self.protocol.send_to_device(opus_data, timestamp)

    async def start(self):
        loop = asyncio.get_event_loop()
        port = self.cfg["udp"]["port"]
        transport, protocol = await loop.create_datagram_endpoint(
            lambda: UdpAudioProtocol(self.state, self._on_decrypted_audio),
            local_addr=(self.cfg["proxy"]["host"], port),
        )
        self.protocol = protocol
        log.info("UDP audio server listening on port %d", port)
