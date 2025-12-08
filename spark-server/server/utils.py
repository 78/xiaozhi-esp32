#!/usr/bin/env python3
"""
Display & Device Control Utilities for Spark AI
Controls the SenseCAP Watcher screen and device state.
"""

import json
import logging
from typing import Optional

logger = logging.getLogger(__name__)

# Supported emotions for Watcher display
EMOTIONS = ["neutral", "happy", "sad", "angry", "fear", "surprise", "thinking"]


async def set_device_state(websocket, emotion: str = "neutral", text: str = ""):
    """
    Controls the Watcher's screen state.

    Args:
        websocket: Active WebSocket connection to device
        emotion: One of: neutral, happy, sad, angry, fear, surprise, thinking
        text: Optional text to display on screen
    """
    if emotion not in EMOTIONS:
        logger.warning(f"Unknown emotion '{emotion}', defaulting to neutral")
        emotion = "neutral"

    cmd = {
        "type": "llm",
        "text": text,
        "emotion": emotion
    }

    try:
        await websocket.send_json(cmd)
        logger.debug(f"Device state: {emotion} - {text[:30] if text else '(no text)'}")
    except Exception as e:
        logger.error(f"Failed to set device state: {e}")


async def send_mcp_command(websocket, method: str, params: Optional[dict] = None, cmd_id: int = 1):
    """
    Send an MCP command to the device.

    Args:
        websocket: Active WebSocket connection
        method: MCP method name (e.g., "camera.take_photo")
        params: Optional parameters dict
        cmd_id: Command ID for tracking responses
    """
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "id": cmd_id
    }
    if params:
        payload["params"] = params

    cmd = {
        "type": "mcp",
        "payload": payload
    }

    try:
        await websocket.send_json(cmd)
        logger.debug(f"MCP command sent: {method}")
    except Exception as e:
        logger.error(f"Failed to send MCP command: {e}")


async def trigger_device_camera(websocket):
    """Convenience function to trigger the device camera."""
    await send_mcp_command(websocket, "camera.take_photo")


async def play_device_sound(websocket, sound: str = "notification"):
    """
    Play a sound on the device.

    Args:
        sound: Sound name (notification, error, success, etc.)
    """
    await send_mcp_command(websocket, "audio.play_sound", {"sound": sound})
