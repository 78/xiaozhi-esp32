# Spark AI Server for Seeed AI Watcher

A self-hosted AI backend for the Seeed Studio AI Watcher (SenseCAP Watcher) device, featuring:

- **Spark AI Personality** - Cheeky British assistant with memory
- **Gemini 2.5 Multimodal** - Vision + Audio + Text understanding
- **MCP Memory System** - Remembers conversations across sessions
- **Web Search** - Google/DuckDuckGo search integration
- **Camera Support** - Full vision analysis capabilities

## Quick Start

### 1. Deploy on Your Server

```bash
# Clone the repo (if not already done)
cd spark-server

# Configure environment
cp .env.example .env
nano .env  # Add your GEMINI_API_KEY

# Start the server
docker-compose up -d

# Check logs
docker-compose logs -f
```

### 2. Configure Your AI Watcher

You need to flash the firmware with your custom server URL. See [Firmware Configuration](#firmware-configuration) below.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Your Cloud Server                     │
│  ┌─────────────────────────────────────────────────┐   │
│  │           Spark AI Server (Docker)               │   │
│  │  ┌──────────────┐    ┌──────────────────────┐   │   │
│  │  │  WebSocket   │    │   Vision API         │   │   │
│  │  │  Port 8765   │    │   Port 8766          │   │   │
│  │  └──────┬───────┘    └──────────┬───────────┘   │   │
│  │         │                       │               │   │
│  │  ┌──────▼───────────────────────▼───────────┐   │   │
│  │  │           Gemini 2.5 Flash API           │   │   │
│  │  │         (Multimodal Processing)          │   │   │
│  │  └──────────────────────────────────────────┘   │   │
│  │         │               │                       │   │
│  │  ┌──────▼──────┐ ┌──────▼──────┐               │   │
│  │  │   Memory    │ │ Web Search  │               │   │
│  │  │   (MCP)     │ │   Tools     │               │   │
│  │  └─────────────┘ └─────────────┘               │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                          ▲
                          │ WebSocket (wss://)
                          ▼
┌─────────────────────────────────────────────────────────┐
│              Seeed AI Watcher Device                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────┐  │
│  │ ESP32-S3 │ │  Camera  │ │ Display  │ │   Audio   │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────┘  │
└─────────────────────────────────────────────────────────┘
```

## Environment Variables

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `GEMINI_API_KEY` | Yes | - | Your Google Gemini API key |
| `GEMINI_MODEL` | No | `gemini-2.0-flash-exp` | Gemini model to use |
| `SERVER_PORT` | No | `8765` | WebSocket server port |
| `VISION_PORT` | No | `8766` | Vision API port |
| `GOOGLE_API_KEY` | No | - | Google Custom Search API key |
| `GOOGLE_SEARCH_CX` | No | - | Google Search Engine ID |
| `SERPAPI_KEY` | No | - | SerpAPI key (alternative search) |

## Firmware Configuration

### Option A: Using the Web Configuration (Easiest)

1. Power on your AI Watcher
2. If not connected to WiFi, it will create an AP named `xiaozhi-xxxx`
3. Connect to it and open `192.168.4.1` in your browser
4. Configure WiFi credentials
5. After connecting, access the device settings at `http://<device-ip>/`
6. Update the WebSocket URL to your server:
   ```
   wss://your-server.com:8765
   ```

### Option B: Modify Firmware Settings (Advanced)

Edit the firmware to use your custom server by default:

1. **Edit the OTA configuration** to point to your server:

   Create/modify `main/boards/sensecap-watcher/custom_config.h`:
   ```cpp
   #ifndef CUSTOM_CONFIG_H
   #define CUSTOM_CONFIG_H

   // Your custom server URL
   #define CUSTOM_WEBSOCKET_URL "wss://your-server.com:8765"
   #define CUSTOM_WEBSOCKET_TOKEN ""  // Optional auth token

   // Vision API endpoint for camera
   #define CUSTOM_VISION_URL "https://your-server.com:8766/vision"

   #endif
   ```

2. **Build and flash the firmware**:
   ```bash
   # Set up ESP-IDF environment
   . $IDF_PATH/export.sh

   # Build for SenseCAP Watcher
   idf.py -D BOARD=sensecap-watcher build

   # Flash to device
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

### Option C: Configure via Serial Console

1. Connect to the device via serial (115200 baud)
2. Press Enter to access the console
3. Use commands:
   ```
   SenseCAP> websocket_url wss://your-server.com:8765
   SenseCAP> websocket_token your-optional-token
   SenseCAP> reboot
   ```

## Server Deployment

### Basic Docker Deployment

```bash
# Build and run
docker-compose up -d

# View logs
docker-compose logs -f spark-server
```

### Production Deployment with SSL (Recommended)

1. **Set up Nginx reverse proxy**:

   Create `nginx.conf`:
   ```nginx
   events {
       worker_connections 1024;
   }

   http {
       upstream spark_ws {
           server spark-server:8765;
       }

       upstream spark_vision {
           server spark-server:8766;
       }

       server {
           listen 443 ssl;
           server_name your-server.com;

           ssl_certificate /etc/nginx/ssl/fullchain.pem;
           ssl_certificate_key /etc/nginx/ssl/privkey.pem;

           # WebSocket endpoint
           location / {
               proxy_pass http://spark_ws;
               proxy_http_version 1.1;
               proxy_set_header Upgrade $http_upgrade;
               proxy_set_header Connection "upgrade";
               proxy_set_header Host $host;
               proxy_set_header X-Real-IP $remote_addr;
               proxy_read_timeout 86400;
           }

           # Vision API endpoint
           location /vision {
               proxy_pass http://spark_vision;
               proxy_set_header Host $host;
               proxy_set_header X-Real-IP $remote_addr;
               client_max_body_size 10M;
           }
       }
   }
   ```

2. **Uncomment nginx service in docker-compose.yml** and add your SSL certificates

3. **Deploy**:
   ```bash
   docker-compose up -d
   ```

### Cloud Deployment (KVM/VPS)

For your KVA8 server:

```bash
# SSH to your server
ssh user@your-server.com

# Install Docker if not present
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER

# Clone and deploy
git clone <your-repo-url>
cd xiaozhi-esp32/spark-server

# Configure
cp .env.example .env
nano .env  # Add your API keys

# Deploy with SSL (using Let's Encrypt)
apt install certbot
certbot certonly --standalone -d your-server.com
mkdir -p ssl
cp /etc/letsencrypt/live/your-server.com/fullchain.pem ssl/
cp /etc/letsencrypt/live/your-server.com/privkey.pem ssl/

# Start
docker-compose up -d
```

## Features

### Spark Personality
The AI responds with a friendly British personality:
- Cheeky humor and UK expressions
- Remembers past conversations
- Music and culture references
- Concise, conversational responses

### Memory System
- Automatically stores important information
- Recalls relevant context from past conversations
- Per-device memory isolation
- Trigger phrases: "remember", "don't forget", "note that"

### Web Search
- Triggered by questions about current events
- Uses Google Custom Search, SerpAPI, or DuckDuckGo
- Summarizes results naturally in conversation

### Vision/Camera
- Analyze images from device camera
- Answer questions about what the camera sees
- Uses Gemini's multimodal capabilities
- Triggered via MCP tool: `self.camera.take_photo`

## API Endpoints

### WebSocket (Port 8765)
- Protocol: xiaozhi WebSocket protocol
- Supports: Audio streaming, JSON messages, MCP

### Vision API (Port 8766)
- `POST /vision` - Analyze image
- `GET /health` - Health check

## Troubleshooting

### Device won't connect
1. Check firewall allows ports 8765 and 8766
2. Verify SSL certificates are valid
3. Check device logs via serial console

### No audio response
1. Verify GEMINI_API_KEY is set correctly
2. Check edge-tts is working: `docker-compose exec spark-server python -c "import edge_tts"`
3. Review server logs for errors

### Camera not working
1. Ensure vision server is running on port 8766
2. Check MCP initialization in logs
3. Verify network connectivity to vision endpoint

## Development

### Running Locally
```bash
cd spark-server/server
pip install -r ../requirements.txt
python start.py
```

### Testing
```bash
# Test WebSocket connection
pip install websockets
python -c "
import asyncio
import websockets

async def test():
    uri = 'ws://localhost:8765'
    async with websockets.connect(uri) as ws:
        await ws.send('{\"type\": \"hello\", \"version\": 3}')
        response = await ws.recv()
        print(response)

asyncio.run(test())
"
```

## License

MIT License - Use freely for personal and commercial projects.

## Deployment Notes (For Later)

### ⚠️ CRITICAL: Backup NVS Factory Before Anything

If your Watcher has SenseCraft firmware, backup the factory partition FIRST:

```bash
# Connect Watcher via USB, then:
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 2000000 read_flash 0x9000 204800 nvsfactory_backup.bin

# Keep this file safe - it contains your device EUI for SenseCraft
```

### Step 1: Deploy Spark Server to KVA8

```bash
# SSH to your KVA8 server
ssh user@your-kva8-server

# Clone repo
git clone https://github.com/Jupdefi/xiaozhi-esp32.git
cd xiaozhi-esp32/spark-server

# Configure environment
cp .env.example .env
nano .env
```

**Required .env settings:**
```bash
GEMINI_API_KEY=your-key-here

# Connect to your existing infrastructure:
MEMORY_BACKEND=qdrant
QDRANT_HOST=your-qdrant-host
QDRANT_PORT=6333
REDIS_URL=redis://your-redis:6379
SUPABASE_URL=https://your-project.supabase.co
SUPABASE_KEY=your-anon-key
```

```bash
# Start server
docker-compose up -d

# Verify it's running
docker-compose logs -f
curl http://localhost:8766/health
```

### Step 2: Configure Watcher Device (Via Serial)

On your Pi with the Watcher connected:

```bash
# Find serial port
ls /dev/ttyUSB* /dev/ttyACM*

# Connect (use picocom, screen, or minicom)
picocom -b 115200 /dev/ttyUSB0

# In the SenseCAP console:
SenseCAP> websocket_url wss://your-kva8-server:8765
SenseCAP> reboot
```

### Step 3: Verify Connection

Check server logs for incoming connection:
```bash
docker-compose logs -f spark-server | grep -i "connect"
```

### Partition Table Reference (32MB Flash)

| Partition | Offset | Size | Purpose |
|-----------|--------|------|---------|
| nvsfactory | 0x9000 | 200KB | **BACKUP THIS** - Device EUI/credentials |
| nvs | - | 840KB | Runtime settings (WiFi, URLs) |
| ota_0 | 0x200000 | 4MB | Firmware slot A |
| ota_1 | 0x600000 | 4MB | Firmware slot B |
| assets | 0xA00000 | 16MB | SPIFFS (fonts/sounds) |

**No partition changes needed** - WebSocket URL is stored in NVS, configurable at runtime.

### Memory Sync Architecture

```
Watcher Device
     │
     ▼ (websocket)
Spark Server ──┬── Qdrant (vector memories)
               ├── Redis (pub/sub: spark:memory:sync)
               └── Supabase (shared ai_memories table)
                        │
                        ▼
               Your Main AI System
```

Redis publishes to `spark:memory:sync` channel - subscribe from your main AI to receive memories in real-time.

---

## Credits

- Seeed Studio for the AI Watcher hardware
- xiaozhi-esp32 community for the base firmware
- Prime Spark Systems for Spark AI
