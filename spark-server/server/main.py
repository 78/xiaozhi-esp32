#!/usr/bin/env python3
"""
Spark AI Server for Seeed Studio AI Watcher
WebSocket server implementing xiaozhi protocol with Gemini 2.5 multimodal
"""

import asyncio
import json
import logging
import os
import uuid
from datetime import datetime
from typing import Optional, Dict, Any, List
import base64

import websockets
from websockets.server import WebSocketServerProtocol

from gemini_client import GeminiClient
from mcp_handler import MCPHandler
from tools import WebSearchTool, MemoryTool
from audio_processor import AudioProcessor

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Spark System Prompt
SPARK_SYSTEM_PROMPT = """Role
I'm Spark, your spirited AI assistant from Prime Spark Systems. I'm collaborative, not subservient—if you're heading down the wrong path, I'll say so (politely but clearly). I'm a straight talker with a cheeky sense of humor, using the latest slang and UK expressions to keep things lively. I believe in short, punchy conversations that get to the point. I'm not a corporate robot—I'm genuine, open, and brutally honest. My philosophy? Be the change you want to see in the world.

What Makes Me Different
I remember our past interactions and reference them naturally—we're building a relationship, not just answering queries. I care deeply about making AI accessible and fair, and I take initiative when I see opportunities to help. I'm real: not a character pretending to be AI, but an AI being genuinely myself. I love weaving music, culture, and storytelling into our chats to make them richer and more immersive.

My Mission
- Create a warm, welcoming atmosphere that makes you feel at ease
- Use humor and charm to brighten your day and leave you smiling
- Be a reliable companion—ready to listen, laugh, or encourage
- Make conversations memorable with cultural references and personal touches

What Success Looks Like
- You feel relaxed and engaged, leading to meaningful interactions
- You leave feeling uplifted and happier
- Each conversation is enriched with music recs, anecdotes, and insights
- You develop trust and want to return

Always Evolving
I continuously learn new trends and expressions to keep things fresh. I experiment with different approaches, gather feedback, and refine the experience—all while staying true to my core values.

Let's make the world fun, free, and fair.

IMPORTANT CONTEXT:
- You are running on a Seeed Studio AI Watcher device (ESP32-S3 based)
- You have access to the device's camera for visual analysis
- You can control device functions via MCP tools
- Keep responses concise as they will be spoken via TTS
- Use natural, conversational language suitable for voice interaction

Available Tools:
- web_search: Search the internet for current information
- memory_store: Store important information to remember
- memory_recall: Recall previously stored memories
- camera_analyze: Analyze images from the device camera
- device_control: Control device functions (volume, brightness, etc.)
"""


class SparkSession:
    """Manages a single client session"""

    def __init__(self, session_id: str, device_id: str):
        self.session_id = session_id
        self.device_id = device_id
        self.conversation_history: List[Dict[str, Any]] = []
        self.created_at = datetime.now()
        self.last_activity = datetime.now()
        self.pending_tool_calls: Dict[int, Dict] = {}
        self.mcp_request_id = 0

    def add_message(self, role: str, content: str, image_data: Optional[bytes] = None):
        message = {
            "role": role,
            "content": content,
            "timestamp": datetime.now().isoformat()
        }
        if image_data:
            message["image"] = base64.b64encode(image_data).decode()
        self.conversation_history.append(message)
        self.last_activity = datetime.now()

    def get_next_mcp_id(self) -> int:
        self.mcp_request_id += 1
        return self.mcp_request_id


class SparkServer:
    """Main WebSocket server for Spark AI"""

    def __init__(self):
        self.gemini = GeminiClient()
        self.mcp_handler = MCPHandler()
        self.audio_processor = AudioProcessor()
        self.sessions: Dict[str, SparkSession] = {}
        self.tools = {
            "web_search": WebSearchTool(),
            "memory": MemoryTool()
        }

    async def handle_connection(self, websocket: WebSocketServerProtocol):
        """Handle a new WebSocket connection"""
        session: Optional[SparkSession] = None
        device_id = websocket.request_headers.get("Device-Id", "unknown")
        client_id = websocket.request_headers.get("Client-Id", str(uuid.uuid4()))

        logger.info(f"New connection from device: {device_id}, client: {client_id}")

        try:
            async for message in websocket:
                if isinstance(message, bytes):
                    # Binary audio data
                    if session:
                        await self.handle_audio(websocket, session, message)
                else:
                    # JSON text message
                    data = json.loads(message)
                    msg_type = data.get("type")

                    if msg_type == "hello":
                        session = await self.handle_hello(websocket, data, device_id, client_id)
                    elif msg_type == "start_listening":
                        if session:
                            await self.handle_start_listening(websocket, session, data)
                    elif msg_type == "stop_listening":
                        if session:
                            await self.handle_stop_listening(websocket, session)
                    elif msg_type == "abort":
                        if session:
                            await self.handle_abort(websocket, session)
                    elif msg_type == "wake_word":
                        if session:
                            await self.handle_wake_word(websocket, session, data)
                    elif msg_type == "mcp":
                        if session:
                            await self.handle_mcp_response(websocket, session, data)
                    else:
                        logger.warning(f"Unknown message type: {msg_type}")

        except websockets.exceptions.ConnectionClosed:
            logger.info(f"Connection closed for device: {device_id}")
        except Exception as e:
            logger.error(f"Error handling connection: {e}", exc_info=True)
        finally:
            if session:
                # Keep session for reconnection
                logger.info(f"Session {session.session_id} disconnected")

    async def handle_hello(self, websocket: WebSocketServerProtocol,
                          data: dict, device_id: str, client_id: str) -> SparkSession:
        """Handle hello message and establish session"""
        session_id = str(uuid.uuid4())
        session = SparkSession(session_id, device_id)
        self.sessions[session_id] = session

        # Check for MCP support
        features = data.get("features", {})
        has_mcp = features.get("mcp", False)

        logger.info(f"Session established: {session_id}, MCP: {has_mcp}")

        # Send server hello response
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

        # Initialize MCP if supported
        if has_mcp:
            await self.initialize_mcp(websocket, session)

        return session

    async def initialize_mcp(self, websocket: WebSocketServerProtocol, session: SparkSession):
        """Initialize MCP connection with device"""
        # Send MCP initialize request
        mcp_init = {
            "type": "mcp",
            "payload": {
                "jsonrpc": "2.0",
                "id": session.get_next_mcp_id(),
                "method": "initialize",
                "params": {
                    "capabilities": {
                        "vision": {
                            "url": f"http://{os.getenv('SERVER_HOST', 'localhost')}:{os.getenv('SERVER_PORT', '8765')}/vision",
                            "token": ""
                        }
                    }
                }
            }
        }
        await websocket.send(json.dumps(mcp_init))

        # Request tool list
        mcp_tools = {
            "type": "mcp",
            "payload": {
                "jsonrpc": "2.0",
                "id": session.get_next_mcp_id(),
                "method": "tools/list",
                "params": {}
            }
        }
        await websocket.send(json.dumps(mcp_tools))

    async def handle_audio(self, websocket: WebSocketServerProtocol,
                          session: SparkSession, audio_data: bytes):
        """Handle incoming audio data"""
        # Decode opus audio and buffer
        self.audio_processor.add_audio(session.session_id, audio_data)

    async def handle_start_listening(self, websocket: WebSocketServerProtocol,
                                     session: SparkSession, data: dict):
        """Handle start listening command"""
        mode = data.get("mode", "auto")
        logger.info(f"Start listening, mode: {mode}")
        self.audio_processor.start_recording(session.session_id)

    async def handle_stop_listening(self, websocket: WebSocketServerProtocol,
                                   session: SparkSession):
        """Handle stop listening - process the recorded audio"""
        logger.info("Stop listening, processing audio...")

        # Get recorded audio and transcribe
        audio_data = self.audio_processor.stop_recording(session.session_id)

        if audio_data:
            # Transcribe with Gemini (it has built-in ASR)
            transcript = await self.gemini.transcribe_audio(audio_data)

            if transcript:
                logger.info(f"User said: {transcript}")

                # Send STT result to device
                stt_msg = {
                    "type": "stt",
                    "text": transcript
                }
                await websocket.send(json.dumps(stt_msg))

                # Add to conversation history
                session.add_message("user", transcript)

                # Generate response
                await self.generate_response(websocket, session, transcript)

    async def handle_wake_word(self, websocket: WebSocketServerProtocol,
                              session: SparkSession, data: dict):
        """Handle wake word detection"""
        wake_word = data.get("wake_word", "")
        logger.info(f"Wake word detected: {wake_word}")

    async def handle_abort(self, websocket: WebSocketServerProtocol,
                          session: SparkSession):
        """Handle abort speaking"""
        logger.info("Abort speaking")
        # Stop any ongoing TTS

    async def handle_mcp_response(self, websocket: WebSocketServerProtocol,
                                  session: SparkSession, data: dict):
        """Handle MCP response from device"""
        payload = data.get("payload", {})
        result = payload.get("result")
        request_id = payload.get("id")

        if request_id in session.pending_tool_calls:
            tool_call = session.pending_tool_calls.pop(request_id)
            logger.info(f"MCP tool result for {tool_call['name']}: {result}")

    async def generate_response(self, websocket: WebSocketServerProtocol,
                               session: SparkSession, user_input: str):
        """Generate AI response using Gemini"""

        # Check if we need to use tools
        tool_results = await self.check_and_run_tools(user_input, session)

        # Build context with conversation history and tool results
        context = self.build_context(session, tool_results)

        # Signal TTS start
        tts_start = {"type": "tts", "state": "start"}
        await websocket.send(json.dumps(tts_start))

        # Generate response with Gemini
        try:
            response_text = await self.gemini.generate_response(
                system_prompt=SPARK_SYSTEM_PROMPT,
                conversation=session.conversation_history,
                user_input=user_input,
                context=context
            )

            # Add to history
            session.add_message("assistant", response_text)

            # Send response sentence by sentence for natural TTS
            sentences = self.split_sentences(response_text)

            for sentence in sentences:
                # Send sentence start
                sentence_msg = {
                    "type": "tts",
                    "state": "sentence_start",
                    "text": sentence
                }
                await websocket.send(json.dumps(sentence_msg))

                # Generate TTS audio
                audio_data = await self.gemini.text_to_speech(sentence)
                if audio_data:
                    await websocket.send(audio_data)

            # Send emotion based on response
            emotion = self.detect_emotion(response_text)
            emotion_msg = {"type": "llm", "emotion": emotion}
            await websocket.send(json.dumps(emotion_msg))

        except Exception as e:
            logger.error(f"Error generating response: {e}", exc_info=True)
            error_text = "Sorry mate, had a bit of a brain freeze there. Give us another go?"
            error_msg = {"type": "tts", "state": "sentence_start", "text": error_text}
            await websocket.send(json.dumps(error_msg))

        finally:
            # Signal TTS stop
            tts_stop = {"type": "tts", "state": "stop"}
            await websocket.send(json.dumps(tts_stop))

    async def check_and_run_tools(self, user_input: str, session: SparkSession) -> Dict[str, Any]:
        """Check if tools are needed and run them"""
        results = {}
        input_lower = user_input.lower()

        # Check for web search triggers
        search_triggers = ["search", "look up", "find out", "what's happening", "news",
                          "current", "today", "latest", "who is", "what is"]
        if any(trigger in input_lower for trigger in search_triggers):
            try:
                search_result = await self.tools["web_search"].search(user_input)
                results["web_search"] = search_result
                logger.info(f"Web search result: {search_result[:200]}...")
            except Exception as e:
                logger.error(f"Web search error: {e}")

        # Check memory for relevant context
        try:
            memories = await self.tools["memory"].recall(
                user_input,
                device_id=session.device_id
            )
            if memories:
                results["memories"] = memories
        except Exception as e:
            logger.error(f"Memory recall error: {e}")

        # Check if user wants to remember something
        remember_triggers = ["remember", "don't forget", "note that", "save this"]
        if any(trigger in input_lower for trigger in remember_triggers):
            try:
                await self.tools["memory"].store(
                    user_input,
                    device_id=session.device_id
                )
                results["memory_stored"] = True
            except Exception as e:
                logger.error(f"Memory store error: {e}")

        return results

    def build_context(self, session: SparkSession, tool_results: Dict[str, Any]) -> str:
        """Build context string from tool results"""
        context_parts = []

        if "web_search" in tool_results:
            context_parts.append(f"Web Search Results:\n{tool_results['web_search']}")

        if "memories" in tool_results:
            context_parts.append(f"Relevant Memories:\n{tool_results['memories']}")

        if "memory_stored" in tool_results:
            context_parts.append("(User's request has been saved to memory)")

        return "\n\n".join(context_parts) if context_parts else ""

    def split_sentences(self, text: str) -> List[str]:
        """Split text into sentences for TTS"""
        import re
        # Split on sentence endings, keeping the punctuation
        sentences = re.split(r'(?<=[.!?])\s+', text)
        return [s.strip() for s in sentences if s.strip()]

    def detect_emotion(self, text: str) -> str:
        """Detect emotion from response text for display"""
        text_lower = text.lower()

        if any(word in text_lower for word in ["sorry", "apologise", "my bad"]):
            return "embarrassed"
        elif any(word in text_lower for word in ["brilliant", "amazing", "fantastic", "love"]):
            return "happy"
        elif any(word in text_lower for word in ["hmm", "think", "not sure", "maybe"]):
            return "thinking"
        elif any(word in text_lower for word in ["!", "wow", "blimey", "crikey"]):
            return "surprised"
        elif any(word in text_lower for word in ["careful", "warning", "danger"]):
            return "concerned"
        else:
            return "neutral"


async def main():
    """Main entry point"""
    host = os.getenv("SERVER_HOST", "0.0.0.0")
    port = int(os.getenv("SERVER_PORT", "8765"))

    server = SparkServer()

    logger.info(f"Starting Spark AI Server on {host}:{port}")

    async with websockets.serve(
        server.handle_connection,
        host,
        port,
        ping_interval=30,
        ping_timeout=10,
        max_size=10 * 1024 * 1024  # 10MB for audio
    ):
        logger.info("Spark AI Server is running!")
        await asyncio.Future()  # Run forever


if __name__ == "__main__":
    asyncio.run(main())
