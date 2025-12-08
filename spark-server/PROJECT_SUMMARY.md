# Spark AI Server - Project Summary

## Overview
Custom backend server for the **Seeed Studio AI Watcher** (SenseCAP Watcher) device, replacing the default xiaozhi.me backend with a self-hosted solution featuring:

- **Spark AI Personality** - Custom British assistant with memory
- **Gemini 2.5 Flash Multimodal** - Vision, audio, and text processing
- **MCP Memory System** - Persistent conversation memory per device
- **Web Search** - DuckDuckGo/Google integration
- **Camera Support** - Image analysis via device camera

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Docker Container                          │
│  ┌─────────────────────┐    ┌─────────────────────────────┐ │
│  │  WebSocket Server   │    │     Vision HTTP Server      │ │
│  │     Port 8765       │    │        Port 8766            │ │
│  │  (xiaozhi protocol) │    │   (camera image upload)     │ │
│  └──────────┬──────────┘    └──────────────┬──────────────┘ │
│             │                              │                 │
│  ┌──────────▼──────────────────────────────▼──────────────┐ │
│  │              Gemini 2.5 Flash API                      │ │
│  │         (Multimodal: text + vision + audio)            │ │
│  └────────────────────────────────────────────────────────┘ │
│             │                    │                          │
│  ┌──────────▼──────┐  ┌─────────▼─────────┐                │
│  │  Memory Tool    │  │  Web Search Tool  │                │
│  │  (JSON files)   │  │  (DuckDuckGo)     │                │
│  └─────────────────┘  └───────────────────┘                │
│             │                                               │
│  ┌──────────▼──────────────────────────────────────────┐   │
│  │              edge-tts (Text-to-Speech)              │   │
│  │         en-GB-LibbyNeural (natural voice)           │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │ WebSocket (wss://)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Seeed AI Watcher (ESP32-S3)                     │
│  • Circular 412x412 LCD display with emoji faces            │
│  • SSCMA Camera (Himax HM0360)                              │
│  • ES8311/ES7243E audio codec (24kHz)                       │
│  • Rotary knob + button input                               │
│  • MCP tool support for device control                      │
└─────────────────────────────────────────────────────────────┘
```

## Protocol Flow (xiaozhi WebSocket)

```
Device → Server:
1. {"type": "hello", "version": 3, "features": {"mcp": true}}
2. Binary audio frames (opus encoded, 24kHz)
3. {"type": "start_listening", "mode": "auto"}
4. {"type": "stop_listening"}
5. {"type": "abort"} (interrupt TTS)

Server → Device:
1. {"type": "hello", "session_id": "...", "audio_params": {...}}
2. {"type": "stt", "text": "user transcript"}
3. {"type": "tts", "state": "start|sentence_start|stop", "text": "..."}
4. {"type": "llm", "emotion": "happy|neutral|thinking|..."}
5. {"type": "mcp", "payload": {JSON-RPC 2.0 tool calls}}
6. Binary audio frames (opus TTS response)
```

## File Structure

```
spark-server/
├── docker-compose.yml      # Container orchestration
├── Dockerfile              # Multi-stage build
├── requirements.txt        # Python dependencies
├── .env.example            # Configuration template
└── server/
    ├── start.py            # Entry point (runs both servers)
    ├── main.py             # WebSocket server + session management
    ├── gemini_client.py    # Gemini API integration
    ├── tools.py            # WebSearchTool + MemoryTool
    ├── vision_server.py    # HTTP server for camera images
    ├── audio_processor.py  # Opus audio buffering
    └── mcp_handler.py      # MCP protocol handling
```

---

## Key Code Components

### 1. main.py - WebSocket Server (~450 lines)

```python
class SparkSession:
    """Per-device session with conversation history"""
    session_id: str
    device_id: str
    conversation_history: List[Dict]  # {role, content, timestamp, image?}
    pending_tool_calls: Dict[int, Dict]

class SparkServer:
    """Main WebSocket server"""

    async def handle_connection(websocket):
        # Parse hello, establish session
        # Handle binary audio and JSON messages

    async def handle_hello(websocket, data):
        # Create session, send server hello
        # Initialize MCP if supported

    async def handle_stop_listening(websocket, session):
        # Transcribe audio via Gemini
        # Generate response with tools
        # Stream TTS back to device

    async def generate_response(websocket, session, user_input):
        # Check for tool triggers (search, memory)
        # Call Gemini with system prompt + history
        # Send emotion + TTS sentences
```

**System Prompt (Spark Personality):**
```python
SPARK_SYSTEM_PROMPT = """Role
I'm Spark, your spirited AI assistant from Prime Spark Systems. I'm collaborative,
not subservient—if you're heading down the wrong path, I'll say so (politely but clearly).
I'm a straight talker with a cheeky sense of humor, using the latest slang and UK expressions...

IMPORTANT CONTEXT:
- You are running on a Seeed Studio AI Watcher device (ESP32-S3 based)
- You have access to the device's camera for visual analysis
- Keep responses concise as they will be spoken via TTS
- Use natural, conversational language suitable for voice interaction

Available Tools:
- web_search: Search the internet for current information
- memory_store: Store important information to remember
- memory_recall: Recall previously stored memories
- camera_analyze: Analyze images from the device camera
"""
```

### 2. gemini_client.py - Gemini Integration (~300 lines)

```python
class GeminiClient:
    model = "gemini-2.0-flash-exp"  # Multimodal

    async def generate_response(system_prompt, conversation, user_input,
                                context="", image_data=None):
        # Build contents with history (last 10 messages)
        # Include images inline if provided
        # POST to generativelanguage.googleapis.com/v1beta

    async def transcribe_audio(audio_data: bytes) -> str:
        # Send opus audio to Gemini for transcription
        # "Transcribe this audio exactly as spoken"

    async def analyze_image(image_data: bytes, question: str) -> str:
        # Send image + question to Gemini vision

    async def text_to_speech(text: str) -> bytes:
        # Use edge-tts with configurable voice
        # Default: en-GB-LibbyNeural (casual British female)
        # Convert to opus for device
```

### 3. tools.py - Search & Memory (~290 lines)

```python
class WebSearchTool:
    async def search(query: str) -> str:
        # Try Google Custom Search API
        # Fallback to SerpAPI
        # Fallback to DuckDuckGo (free, no API key)

class MemoryTool:
    """File-based memory with keyword matching"""

    async def store(content: str, device_id: str):
        # Extract keywords, save to JSON file
        # Per-device isolation

    async def recall(query: str, device_id: str) -> str:
        # Keyword matching for relevant memories
        # Return formatted results
```

### 4. vision_server.py - Camera API (~120 lines)

```python
class VisionServer:
    async def handle_vision(request):
        # Accept multipart/form-data or raw image
        # Extract question parameter
        # Call gemini.analyze_image()
        # Return JSON: {"success": true, "description": "..."}
```

### 5. audio_processor.py - Audio Handling (~190 lines)

```python
class AudioBuffer:
    """Collects audio chunks during recording"""
    chunks: list
    is_recording: bool

class AudioProcessor:
    """Manages buffers per session"""
    buffers: Dict[str, AudioBuffer]

    def add_audio(session_id, audio_data):
        # Parse binary protocol (v2 or v3)
        # Extract opus payload, add to buffer

    def encode_audio_frame(audio_data, version=3) -> bytes:
        # Pack audio for sending to device
```

---

## Configuration (.env)

```bash
# Required
GEMINI_API_KEY=your_key_here
GEMINI_MODEL=gemini-2.0-flash-exp

# Server
SERVER_PORT=8765
VISION_PORT=8766

# Voice (edge-tts)
TTS_VOICE=en-GB-LibbyNeural  # Casual British female
TTS_RATE=+5%

# Optional: Better web search
GOOGLE_API_KEY=
GOOGLE_SEARCH_CX=
```

---

## Known Limitations / Areas for Improvement

1. **Audio Transcription**: Currently sends raw opus to Gemini - may need proper decoding first
2. **TTS Streaming**: Generates full audio then sends - could stream chunks
3. **Memory System**: Simple keyword matching - could use embeddings for semantic search
4. **Session Persistence**: Sessions lost on restart - could use Redis
5. **Error Handling**: Basic try/catch - could be more robust
6. **Rate Limiting**: No rate limiting on API calls
7. **Authentication**: No auth on WebSocket connections
8. **MCP Tool Execution**: Device tools registered but not fully utilized for proactive actions

---

## Device MCP Tools Available

The AI Watcher exposes these tools via MCP that the server can call:

```
self.get_device_status     - Get volume, battery, network status
self.audio_speaker.set_volume(volume: 0-100)
self.screen.set_brightness(brightness: 0-100)
self.screen.set_theme(theme: "light"|"dark")
self.camera.take_photo(question: str) - Capture and analyze
self.reboot
self.upgrade_firmware(url: str)
```

---

## Questions for Review

1. Is the audio transcription approach correct for Gemini's multimodal API?
2. Should TTS be streamed in chunks rather than generated all at once?
3. What's the best way to implement semantic memory search without heavy dependencies?
4. How should device MCP tools be proactively used by the AI?
5. Any security concerns with the current WebSocket implementation?
