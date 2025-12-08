#!/usr/bin/env python3
"""
Spark AI Server Startup Script
Runs WebSocket (Gemini Live bridge) and Vision servers
"""

import asyncio
import logging
import os
import signal
import sys

# Load environment variables
from dotenv import load_dotenv
load_dotenv()

from main import SparkServer
from vision_server import create_vision_app
from aiohttp import web
import websockets

logging.basicConfig(
    level=getattr(logging, os.getenv("LOG_LEVEL", "INFO")),
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


async def run_websocket_server(server: SparkServer):
    """Run the WebSocket server (Gemini Live bridge)"""
    host = os.getenv("SERVER_HOST", "0.0.0.0")
    port = int(os.getenv("SERVER_PORT", "8765"))

    async with websockets.serve(
        server.handle_connection,
        host,
        port,
        ping_interval=30,
        ping_timeout=10,
        max_size=10 * 1024 * 1024
    ):
        logger.info(f"WebSocket server on ws://{host}:{port}")
        await asyncio.Future()


async def run_vision_server():
    """Run the Vision API server"""
    app = await create_vision_app()

    host = os.getenv("VISION_HOST", "0.0.0.0")
    port = int(os.getenv("VISION_PORT", "8766"))

    runner = web.AppRunner(app)
    await runner.setup()

    site = web.TCPSite(runner, host, port)
    await site.start()

    logger.info(f"Vision API on http://{host}:{port}")
    await asyncio.Future()


async def main():
    """Main entry point"""
    logger.info("=" * 50)
    logger.info("  SPARK AI SERVER")
    logger.info("  Gemini Live API + Vector Memory")
    logger.info("  By Prime Spark Systems")
    logger.info("=" * 50)

    # Check required config
    if not os.getenv("GEMINI_API_KEY"):
        logger.error("GEMINI_API_KEY not set!")
        sys.exit(1)

    model = os.getenv("GEMINI_MODEL", "gemini-2.0-flash-exp")
    logger.info(f"Model: {model}")

    # Create server
    server = SparkServer()

    # Run servers
    try:
        await asyncio.gather(
            run_websocket_server(server),
            run_vision_server()
        )
    except Exception as e:
        logger.error(f"Server error: {e}", exc_info=True)
        raise


def handle_shutdown(signum, frame):
    """Handle shutdown signals"""
    logger.info("Shutting down...")
    sys.exit(0)


if __name__ == "__main__":
    signal.signal(signal.SIGINT, handle_shutdown)
    signal.signal(signal.SIGTERM, handle_shutdown)

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Shutdown")
    except Exception as e:
        logger.error(f"Fatal: {e}")
        sys.exit(1)
