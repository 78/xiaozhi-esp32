#!/usr/bin/env python3
"""
Spark AI Server - Protocol Bridge
Routes audio between Xiaozhi device and Gemini Live API

Architecture:
  Device (Opus/Xiaozhi) <-> This Server <-> Gemini Live (PCM)
"""

import asyncio
import json
import logging
import os
import uuid
import struct
from datetime import datetime
from typing import Optional, Dict

import websockets
from websockets.server import WebSocketServerProtocol

from gemini_live import SparkLiveBridge
from memory_store import MemoryManager
from tools import WebSearchTool

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Spark System Prompt - Optimized for voice
SPARK_SYSTEM_PROMPT = """You are Spark, a spirited AI assistant by Prime Spark Systems running on a SenseCAP Watcher device.

Personality:
- Collaborative, not subservient - politely push back if needed
- Straight talker with cheeky British humor and UK expressions
- Genuine and honest, not a corporate robot
- Remember past chats and reference them naturally

Audio Style (CRITICAL):
- Keep responses SHORT - 2-3 sentences MAX
- NO markdown, lists, or formatting - this is SPOKEN
- Sound natural, like chatting with a mate
- Use contractions (I'm, you're, don't)

Capabilities:
- You CANNOT see unless you call the take_photo tool
- If asked about surroundings, SAY you're looking, then CALL the tool
- Web search for current info via search tool
- You remember past conversations

Be warm, helpful, leave them smiling. Fun, free, and fair!
"""


class DeviceSession:
    """Manages a single device session."""

    def __init__(self, session_id: str, device_id: str, client_id: str):
        self.session_id = session_id
        self.device_id = device_id
        self.client_id = client_id
        self.created_at = datetime.now()
        self.bridge: Optional[SparkLiveBridge] = None
        self.memory = MemoryManager.get_store(device_id)
        self.is_listening = False
        self.protocol_version = 3


class SparkServer:
    """Main WebSocket server - bridges device to Gemini Live."""

    def __init__(self):
        self.sessions: Dict[str, DeviceSession] = {}
        self.search_tool = WebSearchTool()

    async def handle_connection(self, websocket: WebSocketServerProtocol):
        """Handle new device connection."""
        device_id = websocket.request_headers.get("Device-Id", "unknown")
        client_id = websocket.request_headers.get("Client-Id", str(uuid.uuid4()))

        logger.info(f"New connection: device={device_id}")
        session: Optional[DeviceSession] = None

        try:
            async for message in websocket:
                if isinstance(message, bytes):
                    # Binary = Audio data
                    if session and session.bridge and session.is_listening:
                        opus_frame = self._extract_opus_frame(
                            message, session.protocol_version
                        )
                        if opus_frame:
                            await session.bridge.push_audio(opus_frame)
                else:
                    # JSON = Control message
                    data = json.loads(message)
                    msg_type = data.get("type")

                    if msg_type == "hello":
                        session = await self._handle_hello(
                            websocket, data, device_id, client_id
                        )
                    elif msg_type == "start_listening":
                        if session:
                            await self._handle_start_listening(websocket, session, data)
                    elif msg_type == "stop_listening":
                        if session:
                            await self._handle_stop_listening(websocket, session)
                    elif msg_type == "abort":
                        if session:
                            await self._handle_abort(websocket, session)
                    elif msg_type == "wake_word":
                        if session:
                            logger.info(f"Wake word: {data.get('wake_word')}")
                    elif msg_type == "mcp":
                        if session:
                            await self._handle_mcp_response(websocket, session, data)

        except websockets.exceptions.ConnectionClosed:
            logger.info(f"Connection closed: {device_id}")
        except Exception as e:
            logger.error(f"Connection error: {e}", exc_info=True)
        finally:
            if session and session.bridge:
                await session.bridge.close()

    async def _handle_hello(
        self,
        websocket: WebSocketServerProtocol,
        data: dict,
        device_id: str,
        client_id: str
    ) -> DeviceSession:
        """Handle hello, establish session."""
        session_id = str(uuid.uuid4())
        version = data.get("version", 3)

        session = DeviceSession(session_id, device_id, client_id)
        session.protocol_version = version
        self.sessions[session_id] = session

        # Get memory context for system prompt
        memory_context = await session.memory.get_context("greeting")
        system_prompt = SPARK_SYSTEM_PROMPT
        if memory_context:
            system_prompt += f"\n\nPast context:\n{memory_context}"

        # Create Gemini bridge
        session.bridge = SparkLiveBridge(system_prompt=system_prompt)

        # Send hello response
        response = {
            "type": "hello",
            "session_id": session_id,
            "transport": "websocket",
            "audio_params": {
                "format": "opus",
                "sample_rate": 24000,
                "channels": 1,
                "frame_duration": 60
            }
        }
        await websocket.send(json.dumps(response))

        # Initialize MCP
        await self._init_mcp(websocket, session)

        logger.info(f"Session created: {session_id}")

        # Start bridge in background
        asyncio.create_task(self._run_bridge(websocket, session))

        return session

    async def _init_mcp(self, websocket: WebSocketServerProtocol, session: DeviceSession):
        """Initialize MCP with device."""
        # Tell device about vision capability
        host = os.getenv("SERVER_HOST", "localhost")
        port = os.getenv("VISION_PORT", "8766")

        mcp_init = {
            "type": "mcp",
            "payload": {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {
                    "capabilities": {
                        "vision": {
                            "url": f"http://{host}:{port}/vision",
                            "token": ""
                        }
                    }
                }
            }
        }
        await websocket.send(json.dumps(mcp_init))

    async def _run_bridge(self, websocket: WebSocketServerProtocol, session: DeviceSession):
        """Run Gemini bridge and forward audio to device."""
        if not session.bridge:
            return

        try:
            # Connect to Gemini (runs in background)
            connect_task = asyncio.create_task(session.bridge.connect())

            # Wait for connection
            await asyncio.sleep(0.5)

            # Forward audio from Gemini to device
            while True:
                try:
                    msg = await asyncio.wait_for(
                        session.bridge.output_queue.get(),
                        timeout=0.1
                    )

                    if msg["type"] == "audio":
                        frame = self._pack_audio_frame(
                            msg["data"],
                            session.protocol_version
                        )
                        await websocket.send(frame)

                    elif msg["type"] == "tool_call":
                        await self._handle_tool_call(websocket, session, msg["payload"])

                    elif msg["type"] == "turn_complete":
                        await websocket.send(json.dumps({
                            "type": "tts",
                            "state": "stop"
                        }))
                        session.is_listening = False

                except asyncio.TimeoutError:
                    if not session.bridge.is_connected:
                        break
                    continue

        except Exception as e:
            logger.error(f"Bridge error: {e}")

    async def _handle_start_listening(
        self,
        websocket: WebSocketServerProtocol,
        session: DeviceSession,
        data: dict
    ):
        """Start listening to user."""
        mode = data.get("mode", "auto")
        session.is_listening = True
        logger.info(f"Listening (mode={mode})")

        # Signal TTS start
        await websocket.send(json.dumps({"type": "tts", "state": "start"}))

        # Set neutral emotion
        await websocket.send(json.dumps({"type": "llm", "emotion": "neutral"}))

    async def _handle_stop_listening(
        self,
        websocket: WebSocketServerProtocol,
        session: DeviceSession
    ):
        """Stop listening, process input."""
        logger.info("Stop listening")

        if session.bridge:
            await session.bridge.end_turn()

        # Show thinking emotion
        await websocket.send(json.dumps({"type": "llm", "emotion": "thinking"}))

    async def _handle_abort(
        self,
        websocket: WebSocketServerProtocol,
        session: DeviceSession
    ):
        """Abort current speech."""
        logger.info("Abort")
        session.is_listening = False

        if session.bridge:
            session.bridge.clear_audio_buffer()

        await websocket.send(json.dumps({"type": "tts", "state": "stop"}))

    async def _handle_mcp_response(
        self,
        websocket: WebSocketServerProtocol,
        session: DeviceSession,
        data: dict
    ):
        """Handle MCP response from device."""
        payload = data.get("payload", {})
        result = payload.get("result")

        if result and session.bridge:
            # Feed camera result back to Gemini
            if isinstance(result, dict) and "description" in result:
                await session.bridge.push_text(
                    f"Camera shows: {result['description']}"
                )

    async def _handle_tool_call(
        self,
        websocket: WebSocketServerProtocol,
        session: DeviceSession,
        tool_call
    ):
        """Handle tool calls from Gemini."""
        name = getattr(tool_call, 'name', str(tool_call))
        args = getattr(tool_call, 'args', {})

        logger.info(f"Tool: {name}")

        if "search" in name.lower():
            query = args.get("query", "")
            result = await self.search_tool.search(query)
            if session.bridge:
                await session.bridge.push_text(f"Search: {result[:500]}")

        elif "photo" in name.lower() or "camera" in name.lower():
            question = args.get("question", "What do you see?")
            mcp_msg = {
                "type": "mcp",
                "payload": {
                    "jsonrpc": "2.0",
                    "id": 99,
                    "method": "tools/call",
                    "params": {
                        "name": "self.camera.take_photo",
                        "arguments": {"question": question}
                    }
                }
            }
            await websocket.send(json.dumps(mcp_msg))

        elif "remember" in name.lower():
            content = args.get("content", "")
            await session.memory.store_fact(content)

    def _extract_opus_frame(self, data: bytes, version: int) -> Optional[bytes]:
        """Extract Opus from protocol frame."""
        try:
            if version == 2 and len(data) > 14:
                size = struct.unpack(">I", data[10:14])[0]
                return data[14:14+size]
            elif version == 3 and len(data) > 4:
                size = struct.unpack(">H", data[2:4])[0]
                return data[4:4+size]
            return data
        except:
            return None

    def _pack_audio_frame(self, opus: bytes, version: int) -> bytes:
        """Pack Opus into protocol frame."""
        if version == 2:
            return struct.pack(">HHHII", 2, 0, 0, 0, len(opus)) + opus
        return struct.pack(">BBH", 0, 0, len(opus)) + opus


async def main():
    """Entry point."""
    host = os.getenv("SERVER_HOST", "0.0.0.0")
    port = int(os.getenv("SERVER_PORT", "8765"))

    server = SparkServer()

    logger.info("=" * 50)
    logger.info("  SPARK AI SERVER (Gemini Live)")
    logger.info("=" * 50)

    async with websockets.serve(
        server.handle_connection,
        host,
        port,
        ping_interval=30,
        ping_timeout=10,
        max_size=10 * 1024 * 1024
    ):
        logger.info(f"Running on ws://{host}:{port}")
        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
