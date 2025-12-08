#!/usr/bin/env python3
"""
Gemini Live API Client for Spark AI Server
Bidirectional audio streaming with real-time transcoding

Flow:
  Device (Opus 24kHz) -> Server (PCM 16kHz) -> Gemini Live
  Gemini Live (PCM 24kHz) -> Server (Opus 24kHz) -> Device
"""

import asyncio
import os
import logging
import numpy as np
from typing import Optional, Callable, Any
from scipy import signal

logger = logging.getLogger(__name__)

# Configuration
GEMINI_MODEL = os.getenv("GEMINI_MODEL", "gemini-2.0-flash-exp")
API_KEY = os.getenv("GEMINI_API_KEY", "")

# Audio Configuration
DEVICE_SAMPLE_RATE = 24000    # Watcher sends/receives 24kHz
GEMINI_INPUT_RATE = 16000     # Gemini expects 16kHz input
GEMINI_OUTPUT_RATE = 24000    # Gemini outputs 24kHz
OPUS_FRAME_MS = 60            # 60ms frames (matches Xiaozhi)
OPUS_FRAME_SAMPLES = int(DEVICE_SAMPLE_RATE * OPUS_FRAME_MS / 1000)  # 1440 samples


class PCMBuffer:
    """
    Thread-safe buffer that accumulates raw PCM bytes from Gemini
    and yields fixed-size chunks required by the Opus encoder.
    """

    def __init__(self, sample_width: int = 2):
        self.buffer = bytearray()
        self.sample_width = sample_width  # 2 bytes (16-bit PCM)

    def add_data(self, data: bytes):
        """Add new raw PCM bytes from Gemini to the buffer."""
        if data:
            self.buffer.extend(data)

    def get_chunk(self, frame_size_samples: int) -> Optional[bytes]:
        """
        Retrieve a chunk of specific size (in samples) if available.
        Returns None if not enough data is buffered.
        """
        required_bytes = frame_size_samples * self.sample_width

        if len(self.buffer) >= required_bytes:
            chunk = bytes(self.buffer[:required_bytes])
            self.buffer = self.buffer[required_bytes:]
            return chunk
        return None

    def clear(self):
        """Wipe buffer (useful when user interrupts)."""
        self.buffer = bytearray()

    def __len__(self):
        return len(self.buffer)


class SparkLiveBridge:
    """
    Bidirectional bridge between Xiaozhi device and Gemini Live API.
    Handles all audio transcoding (Opus <-> PCM) and resampling.
    """

    def __init__(self, system_prompt: str = ""):
        self.system_prompt = system_prompt
        self.client = None
        self.session = None
        self.output_queue: asyncio.Queue = asyncio.Queue()
        self.is_connected = False

        # Audio Processing
        self.opus_decoder = None
        self.opus_encoder = None
        self.pcm_buffer = PCMBuffer()

        # Resampling ratio (24kHz -> 16kHz)
        self.resample_ratio = GEMINI_INPUT_RATE / DEVICE_SAMPLE_RATE

        # Callbacks
        self.on_transcript: Optional[Callable[[str], Any]] = None
        self.on_tool_call: Optional[Callable[[dict], Any]] = None
        self.on_turn_complete: Optional[Callable[[], Any]] = None

        logger.info("SparkLiveBridge initialized")

    def _init_opus(self):
        """Lazy initialization of Opus codec."""
        try:
            import opuslib
            if self.opus_decoder is None:
                self.opus_decoder = opuslib.Decoder(DEVICE_SAMPLE_RATE, 1)
            if self.opus_encoder is None:
                self.opus_encoder = opuslib.Encoder(DEVICE_SAMPLE_RATE, 1, 'voip')
            return True
        except ImportError:
            logger.error("opuslib not installed! Run: pip install opuslib")
            return False
        except Exception as e:
            logger.error(f"Opus init failed: {e}")
            return False

    async def connect(self, tools: Optional[list] = None):
        """Start bidirectional Gemini Live session."""
        if not API_KEY:
            raise ValueError("GEMINI_API_KEY not set!")

        if not self._init_opus():
            raise RuntimeError("Failed to initialize Opus codec")

        try:
            from google import genai

            self.client = genai.Client(
                api_key=API_KEY,
                http_options={"api_version": "v1alpha"}
            )

            # Configure session
            config = {
                "response_modalities": ["AUDIO"],
                "system_instruction": self.system_prompt
            }
            if tools:
                config["tools"] = tools

            logger.info(f"Connecting to Gemini Live ({GEMINI_MODEL})...")

            async with self.client.aio.live.connect(
                model=GEMINI_MODEL,
                config=config
            ) as session:
                self.session = session
                self.is_connected = True
                logger.info("Connected to Gemini Live!")

                # Run receive loop
                await self._receive_loop()

        except Exception as e:
            logger.error(f"Gemini connection failed: {e}")
            raise
        finally:
            self.is_connected = False
            self.session = None

    async def push_audio(self, opus_frame: bytes):
        """
        Process audio from device:
        1. Decode Opus -> PCM (24kHz)
        2. Resample PCM (24kHz -> 16kHz)
        3. Send to Gemini
        """
        if not self.session or not self.opus_decoder:
            return

        try:
            # 1. Decode Opus to PCM (24kHz)
            pcm_data = self.opus_decoder.decode(opus_frame, OPUS_FRAME_SAMPLES)

            # 2. Resample 24kHz -> 16kHz for Gemini
            audio_np = np.frombuffer(pcm_data, dtype=np.int16)
            num_samples = int(len(audio_np) * self.resample_ratio)
            resampled = signal.resample(audio_np, num_samples).astype(np.int16)

            # 3. Send to Gemini Live
            await self.session.send(
                input={
                    "data": resampled.tobytes(),
                    "mime_type": "audio/pcm"
                },
                end_of_turn=False
            )

        except Exception as e:
            logger.error(f"Audio push error: {e}")

    async def push_text(self, text: str, end_turn: bool = True):
        """Send text input to Gemini."""
        if self.session:
            await self.session.send(input=text, end_of_turn=end_turn)

    async def push_image(self, image_data: bytes, question: str = "What do you see?"):
        """Send image to Gemini for analysis."""
        if self.session:
            import base64
            await self.session.send(
                input=[
                    {"mime_type": "image/jpeg", "data": base64.b64encode(image_data).decode()},
                    question
                ],
                end_of_turn=True
            )

    async def end_turn(self):
        """Signal end of user turn."""
        if self.session:
            await self.session.send(input="", end_of_turn=True)

    def clear_audio_buffer(self):
        """Clear pending audio (for interruptions)."""
        self.pcm_buffer.clear()
        # Also clear the output queue
        while not self.output_queue.empty():
            try:
                self.output_queue.get_nowait()
            except asyncio.QueueEmpty:
                break

    async def _receive_loop(self):
        """
        Receive audio/text from Gemini:
        1. Receive PCM (24kHz)
        2. Buffer until we have full frame
        3. Encode to Opus
        4. Queue for WebSocket
        """
        while self.is_connected:
            try:
                async for response in self.session.receive():
                    server_content = response.server_content
                    if server_content is None:
                        continue

                    model_turn = server_content.model_turn
                    if model_turn:
                        for part in model_turn.parts:
                            # Handle Audio Output
                            if part.inline_data:
                                await self._process_audio_output(part.inline_data.data)

                            # Handle Text (transcripts, thoughts)
                            if part.text:
                                logger.debug(f"Gemini text: {part.text}")
                                if self.on_transcript:
                                    await self._safe_callback(
                                        self.on_transcript, part.text
                                    )

                            # Handle Tool Calls
                            if part.function_call:
                                logger.info(f"Tool call: {part.function_call}")
                                await self.output_queue.put({
                                    "type": "tool_call",
                                    "payload": part.function_call
                                })
                                if self.on_tool_call:
                                    await self._safe_callback(
                                        self.on_tool_call, part.function_call
                                    )

                    # Turn complete signal
                    if server_content.turn_complete:
                        await self.output_queue.put({"type": "turn_complete"})
                        if self.on_turn_complete:
                            await self._safe_callback(self.on_turn_complete)

            except asyncio.CancelledError:
                logger.info("Receive loop cancelled")
                break
            except Exception as e:
                logger.error(f"Receive loop error: {e}")
                break

    async def _process_audio_output(self, pcm_data: bytes):
        """Buffer PCM and encode complete frames to Opus."""
        self.pcm_buffer.add_data(pcm_data)

        # Extract and encode complete frames
        while True:
            chunk = self.pcm_buffer.get_chunk(OPUS_FRAME_SAMPLES)
            if chunk is None:
                break

            try:
                opus_frame = self.opus_encoder.encode(chunk, OPUS_FRAME_SAMPLES)
                await self.output_queue.put({
                    "type": "audio",
                    "data": opus_frame
                })
            except Exception as e:
                logger.error(f"Opus encode error: {e}")

    async def _safe_callback(self, callback, *args):
        """Safely execute callback (sync or async)."""
        try:
            result = callback(*args)
            if asyncio.iscoroutine(result):
                await result
        except Exception as e:
            logger.error(f"Callback error: {e}")

    async def close(self):
        """Clean shutdown."""
        self.is_connected = False
        self.clear_audio_buffer()
        logger.info("SparkLiveBridge closed")
