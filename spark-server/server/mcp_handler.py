#!/usr/bin/env python3
"""
MCP (Model Context Protocol) Handler for Spark AI Server
Handles tool discovery and execution on the AI Watcher device
"""

import logging
from typing import Dict, Any, List, Optional, Callable
import json

logger = logging.getLogger(__name__)


class MCPTool:
    """Represents an MCP tool"""

    def __init__(self, name: str, description: str, parameters: Dict[str, Any]):
        self.name = name
        self.description = description
        self.parameters = parameters


class MCPHandler:
    """Handles MCP protocol interactions with the device"""

    def __init__(self):
        self.device_tools: Dict[str, List[MCPTool]] = {}  # device_id -> tools
        self.pending_responses: Dict[int, asyncio.Future] = {}

    def register_device_tools(self, device_id: str, tools_json: Dict):
        """Register tools available on a device"""
        tools = []
        for tool_data in tools_json.get("tools", []):
            tool = MCPTool(
                name=tool_data.get("name", ""),
                description=tool_data.get("description", ""),
                parameters=tool_data.get("inputSchema", {})
            )
            tools.append(tool)
            logger.info(f"Registered tool for {device_id}: {tool.name}")

        self.device_tools[device_id] = tools

    def get_device_tools(self, device_id: str) -> List[MCPTool]:
        """Get available tools for a device"""
        return self.device_tools.get(device_id, [])

    def get_tools_description(self, device_id: str) -> str:
        """Get a formatted description of available tools for the LLM"""
        tools = self.get_device_tools(device_id)

        if not tools:
            return "No device tools available."

        descriptions = ["Available device tools:"]
        for tool in tools:
            desc = f"\n- {tool.name}: {tool.description}"
            if tool.parameters.get("properties"):
                params = ", ".join(tool.parameters["properties"].keys())
                desc += f" (params: {params})"
            descriptions.append(desc)

        return "\n".join(descriptions)

    def build_tool_call(self, request_id: int, tool_name: str, arguments: Dict[str, Any]) -> Dict:
        """Build an MCP tool call message"""
        return {
            "type": "mcp",
            "payload": {
                "jsonrpc": "2.0",
                "id": request_id,
                "method": "tools/call",
                "params": {
                    "name": tool_name,
                    "arguments": arguments
                }
            }
        }

    def parse_tool_response(self, response: Dict) -> Optional[Any]:
        """Parse a tool call response"""
        payload = response.get("payload", {})

        if "error" in payload:
            logger.error(f"Tool call error: {payload['error']}")
            return None

        result = payload.get("result")
        return result


# Common device tool names for reference
DEVICE_TOOLS = {
    "get_device_status": "self.get_device_status",
    "set_volume": "self.audio_speaker.set_volume",
    "set_brightness": "self.screen.set_brightness",
    "set_theme": "self.screen.set_theme",
    "take_photo": "self.camera.take_photo",
    "get_system_info": "self.get_system_info",
    "reboot": "self.reboot"
}


import asyncio
