#!/usr/bin/env python3
"""
Spark AI Server - Production Orchestrator
Routes audio between Xiaozhi device and Gemini Live API

Architecture:
  Device (Opus/Xiaozhi) <-> FastAPI WebSocket <-> Gemini Live (PCM)
"""

import asyncio
import json
import logging
import os
import struct
import threading
from typing import Optional

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse

from gemini_live import SparkLiveBridge
from memory_store import MemoryManager
from tools import WebSearchTool
from vision_server import start_vision_server, image_ready_event, LATEST_IMAGE_PATH
from utils import set_device_state, send_mcp_command

# Logging
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

# Tool definitions for Gemini
TOOL_DEFINITIONS = [
    {
        "function_declarations": [
            {
                "name": "take_photo",
                "description": "Take a photo with the device camera to see surroundings",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "question": {
                            "type": "string",
                            "description": "What to look for or analyze in the image"
                        }
                    }
                }
            },
            {
                "name": "web_search",
                "description": "Search the web for current information",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "query": {
                            "type": "string",
                            "description": "Search query"
                        }
                    },
                    "required": ["query"]
                }
            },
            {
                "name": "remember",
                "description": "Store important information for later recall",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "content": {
                            "type": "string",
                            "description": "Information to remember"
                        }
                    },
                    "required": ["content"]
                }
            }
        ]
    }
]

app = FastAPI(title="Spark AI Server")
search_tool = WebSearchTool()


@app.on_event("startup")
async def startup_event():
    """Start vision server on startup."""
    threading.Thread(target=start_vision_server, daemon=True).start()
    logger.info("Vision server started")


@app.get("/health")
async def health():
    """Health check endpoint."""
    return JSONResponse({"status": "healthy", "service": "spark-ai"})


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """Main WebSocket endpoint for device connections."""
    await websocket.accept()

    # Extract device info from headers
    device_id = websocket.headers.get("device-id", "unknown")
    client_id = websocket.headers.get("client-id", "unknown")
    logger.info(f"Device connected: {device_id}")

    # Initialize memory manager
    memory = MemoryManager(device_id)

    # 1. COLD START: Load Hive Mind Context
    context_str = await memory.get_system_context()
    system_prompt = SPARK_SYSTEM_PROMPT
    if context_str:
        system_prompt += f"\n\nPast context:\n{context_str}"

    # 2. INIT BRIDGE: Setup Gemini Live
    bridge = SparkLiveBridge(system_prompt=system_prompt)

    # Protocol state
    protocol_version = 3
    is_listening = False

    # Helper: Handle tool calls
    async def handle_tool(tool_call):
        """Process tool calls from Gemini."""
        nonlocal is_listening

        fname = getattr(tool_call, 'name', str(tool_call))
        args = getattr(tool_call, 'args', {})

        logger.info(f"Tool call: {fname}")

        # Update screen -> Thinking
        await set_device_state(websocket, emotion="thinking")

        if fname == "take_photo":
            # Vision Flow
            await set_device_state(websocket, emotion="thinking", text="Looking...")
            image_ready_event.clear()

            # Trigger device camera via MCP
            await send_mcp_command(websocket, "tools/call", {
                "name": "self.camera.take_photo",
                "arguments": {"question": args.get("question", "What do you see?")}
            }, cmd_id=99)

            try:
                # Wait for upload (10s timeout)
                await asyncio.wait_for(image_ready_event.wait(), timeout=10.0)
                await set_device_state(websocket, emotion="happy", text="Got it!")

                # Send image to Gemini
                if os.path.exists(LATEST_IMAGE_PATH):
                    await bridge.send_image(LATEST_IMAGE_PATH)
            except asyncio.TimeoutError:
                await set_device_state(websocket, emotion="sad", text="Camera timeout")
                await bridge.push_text("System: Camera timed out, couldn't get image.")

        elif fname == "web_search":
            query = args.get("query", "")
            logger.info(f"Web search: {query}")
            try:
                result = await search_tool.search(query)
                await bridge.push_text(f"Search results for '{query}': {result[:1000]}")
            except Exception as e:
                logger.error(f"Search failed: {e}")
                await bridge.push_text(f"Search failed: {str(e)}")

        elif fname == "remember":
            content = args.get("content", "")
            if content:
                await memory.store(content, {"type": "user_fact"})
                await bridge.push_text(f"Noted: {content}")
                logger.info(f"Stored memory: {content[:50]}...")

    # Helper: Forward Gemini output to device
    async def forward_to_device():
        """Forward audio and tool calls from Gemini to device."""
        nonlocal is_listening

        while True:
            try:
                msg = await asyncio.wait_for(bridge.output_queue.get(), timeout=0.1)

                if msg["type"] == "audio":
                    # Pack and send audio
                    opus_data = msg["data"]
                    frame = pack_audio_frame(opus_data, protocol_version)
                    await websocket.send_bytes(frame)

                    # Show happy face when speaking
                    await set_device_state(websocket, emotion="happy")

                elif msg["type"] == "tool_call":
                    # Handle tool calls
                    payload = msg["payload"]
                    if hasattr(payload, 'function_calls'):
                        for call in payload.function_calls:
                            await handle_tool(call)
                    else:
                        await handle_tool(payload)

                elif msg["type"] == "turn_complete":
                    # Gemini finished speaking
                    await websocket.send_text(json.dumps({
                        "type": "tts",
                        "state": "stop"
                    }))
                    is_listening = False
                    await set_device_state(websocket, emotion="neutral")

            except asyncio.TimeoutError:
                continue
            except Exception as e:
                logger.error(f"Forward error: {e}")
                break

    # Helper: Process incoming device messages
    async def handle_device_message(data):
        """Handle JSON messages from device."""
        nonlocal protocol_version, is_listening

        msg_type = data.get("type")

        if msg_type == "hello":
            protocol_version = data.get("version", 3)
            logger.info(f"Hello from device (protocol v{protocol_version})")

            # Send hello response
            response = {
                "type": "hello",
                "session_id": f"spark-{device_id}",
                "transport": "websocket",
                "audio_params": {
                    "format": "opus",
                    "sample_rate": 24000,
                    "channels": 1,
                    "frame_duration": 60
                }
            }
            await websocket.send_text(json.dumps(response))

            # Initialize MCP
            vision_host = os.getenv("VISION_HOST", "localhost")
            vision_port = os.getenv("VISION_PORT", "8766")
            await send_mcp_command(websocket, "initialize", {
                "capabilities": {
                    "vision": {
                        "url": f"http://{vision_host}:{vision_port}/vision",
                        "token": ""
                    }
                }
            }, cmd_id=1)

        elif msg_type == "start_listening":
            is_listening = True
            logger.info("Start listening")

            await websocket.send_text(json.dumps({"type": "tts", "state": "start"}))
            await set_device_state(websocket, emotion="neutral", text="Listening...")

        elif msg_type == "stop_listening":
            logger.info("Stop listening")

            if bridge:
                await bridge.end_turn()

            await set_device_state(websocket, emotion="thinking")

        elif msg_type == "abort":
            logger.info("Abort")
            is_listening = False

            if bridge:
                bridge.clear_audio_buffer()

            await websocket.send_text(json.dumps({"type": "tts", "state": "stop"}))
            await set_device_state(websocket, emotion="neutral")

        elif msg_type == "mcp":
            # MCP response from device
            payload = data.get("payload", {})
            result = payload.get("result")

            if result and bridge:
                if isinstance(result, dict) and "description" in result:
                    await bridge.push_text(f"Camera shows: {result['description']}")

    # Main connection loop
    try:
        # Start Gemini connection
        gemini_task = asyncio.create_task(bridge.connect())
        await asyncio.sleep(0.3)  # Let connection establish

        # Start forwarding task
        forward_task = asyncio.create_task(forward_to_device())

        while True:
            data = await websocket.receive()

            if "bytes" in data:
                # Binary = Audio from device
                if is_listening and bridge:
                    opus_frame = extract_opus_frame(data["bytes"], protocol_version)
                    if opus_frame:
                        await bridge.push_audio(opus_frame)

            elif "text" in data:
                # JSON = Control message
                try:
                    msg = json.loads(data["text"])
                    await handle_device_message(msg)
                except json.JSONDecodeError:
                    logger.warning(f"Invalid JSON: {data['text'][:100]}")

    except WebSocketDisconnect:
        logger.info(f"Device disconnected: {device_id}")
    except Exception as e:
        logger.error(f"Connection error: {e}", exc_info=True)
    finally:
        # Cleanup
        if 'gemini_task' in locals():
            gemini_task.cancel()
        if 'forward_task' in locals():
            forward_task.cancel()
        if bridge:
            await bridge.close()
        logger.info(f"Session cleanup: {device_id}")


def extract_opus_frame(data: bytes, version: int) -> Optional[bytes]:
    """Extract Opus frame from protocol wrapper."""
    try:
        if version == 2 and len(data) > 14:
            size = struct.unpack(">I", data[10:14])[0]
            return data[14:14+size]
        elif version == 3 and len(data) > 4:
            size = struct.unpack(">H", data[2:4])[0]
            return data[4:4+size]
        return data
    except Exception:
        return None


def pack_audio_frame(opus: bytes, version: int) -> bytes:
    """Pack Opus frame into protocol wrapper."""
    if version == 2:
        return struct.pack(">HHHII", 2, 0, 0, 0, len(opus)) + opus
    return struct.pack(">BBH", 0, 0, len(opus)) + opus


# Standalone run support
if __name__ == "__main__":
    import uvicorn

    host = os.getenv("SERVER_HOST", "0.0.0.0")
    port = int(os.getenv("SERVER_PORT", "8765"))

    logger.info("=" * 50)
    logger.info("  SPARK AI SERVER (Gemini Live + FastAPI)")
    logger.info("=" * 50)
    logger.info(f"WebSocket: ws://{host}:{port}/ws")

    uvicorn.run(app, host=host, port=port)
