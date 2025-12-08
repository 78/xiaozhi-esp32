#!/usr/bin/env python3
"""
Audio Processor for Spark AI Server
Handles audio buffering, encoding, and decoding
"""

import logging
from typing import Dict, Optional
from collections import defaultdict
import struct
import io

logger = logging.getLogger(__name__)


class AudioBuffer:
    """Buffer for collecting audio chunks"""

    def __init__(self):
        self.chunks: list = []
        self.is_recording = False
        self.sample_rate = 24000
        self.channels = 1
        self.frame_duration = 60  # ms

    def add_chunk(self, data: bytes):
        """Add an audio chunk to the buffer"""
        if self.is_recording:
            self.chunks.append(data)

    def start_recording(self):
        """Start recording"""
        self.chunks = []
        self.is_recording = True
        logger.debug("Started recording")

    def stop_recording(self) -> bytes:
        """Stop recording and return all audio data"""
        self.is_recording = False
        data = b"".join(self.chunks)
        self.chunks = []
        logger.debug(f"Stopped recording, total bytes: {len(data)}")
        return data

    def clear(self):
        """Clear the buffer"""
        self.chunks = []


class AudioProcessor:
    """Manages audio processing for multiple sessions"""

    def __init__(self):
        self.buffers: Dict[str, AudioBuffer] = defaultdict(AudioBuffer)

    def add_audio(self, session_id: str, audio_data: bytes):
        """Add audio data to a session's buffer"""
        # Parse the binary protocol (version 2 or 3)
        # The device sends opus-encoded audio

        if len(audio_data) < 4:
            return

        # Try to detect protocol version
        # Version 2: 2 bytes version + 2 bytes type + 2 bytes reserved + 4 bytes timestamp + 4 bytes size
        # Version 3: 1 byte type + 1 byte reserved + 2 bytes size

        # Simple heuristic: if first 2 bytes look like version number
        first_short = struct.unpack(">H", audio_data[:2])[0]

        if first_short in (2, 3):
            # Protocol version 2
            if len(audio_data) > 14:
                payload = audio_data[14:]
                self.buffers[session_id].add_chunk(payload)
        else:
            # Protocol version 3 or raw
            if len(audio_data) > 4:
                # Version 3: type(1) + reserved(1) + size(2)
                payload_size = struct.unpack(">H", audio_data[2:4])[0]
                if len(audio_data) >= 4 + payload_size:
                    payload = audio_data[4:4+payload_size]
                    self.buffers[session_id].add_chunk(payload)
                else:
                    # Raw opus data
                    self.buffers[session_id].add_chunk(audio_data)
            else:
                self.buffers[session_id].add_chunk(audio_data)

    def start_recording(self, session_id: str):
        """Start recording for a session"""
        self.buffers[session_id].start_recording()

    def stop_recording(self, session_id: str) -> bytes:
        """Stop recording and return audio data"""
        return self.buffers[session_id].stop_recording()

    def clear_buffer(self, session_id: str):
        """Clear a session's buffer"""
        self.buffers[session_id].clear()

    def encode_audio_frame(self, audio_data: bytes, timestamp: int = 0, version: int = 3) -> bytes:
        """Encode audio data for sending to device"""

        if version == 2:
            # Protocol version 2
            header = struct.pack(">HHHII",
                2,  # version
                0,  # type (audio)
                0,  # reserved
                timestamp,
                len(audio_data)
            )
            return header + audio_data
        else:
            # Protocol version 3
            header = struct.pack(">BBH",
                0,  # type (audio)
                0,  # reserved
                len(audio_data)
            )
            return header + audio_data


class OpusDecoder:
    """Simple opus decoder wrapper"""

    def __init__(self, sample_rate: int = 24000, channels: int = 1):
        self.sample_rate = sample_rate
        self.channels = channels
        self._decoder = None

    def _get_decoder(self):
        """Lazy initialization of opus decoder"""
        if self._decoder is None:
            try:
                import opuslib
                self._decoder = opuslib.Decoder(self.sample_rate, self.channels)
            except ImportError:
                logger.warning("opuslib not available, audio decoding disabled")
        return self._decoder

    def decode(self, opus_data: bytes) -> Optional[bytes]:
        """Decode opus data to PCM"""
        decoder = self._get_decoder()
        if decoder is None:
            return None

        try:
            # Decode opus frame
            pcm = decoder.decode(opus_data, frame_size=960)  # 20ms at 48kHz
            return pcm
        except Exception as e:
            logger.error(f"Opus decode error: {e}")
            return None


class OpusEncoder:
    """Simple opus encoder wrapper"""

    def __init__(self, sample_rate: int = 24000, channels: int = 1, bitrate: int = 24000):
        self.sample_rate = sample_rate
        self.channels = channels
        self.bitrate = bitrate
        self._encoder = None

    def _get_encoder(self):
        """Lazy initialization of opus encoder"""
        if self._encoder is None:
            try:
                import opuslib
                self._encoder = opuslib.Encoder(
                    self.sample_rate,
                    self.channels,
                    opuslib.APPLICATION_AUDIO
                )
                self._encoder.bitrate = self.bitrate
            except ImportError:
                logger.warning("opuslib not available, audio encoding disabled")
        return self._encoder

    def encode(self, pcm_data: bytes, frame_size: int = 960) -> Optional[bytes]:
        """Encode PCM data to opus"""
        encoder = self._get_encoder()
        if encoder is None:
            return None

        try:
            opus = encoder.encode(pcm_data, frame_size)
            return opus
        except Exception as e:
            logger.error(f"Opus encode error: {e}")
            return None
