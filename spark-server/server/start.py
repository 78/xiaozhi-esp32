#!/usr/bin/env python3
"""
Spark AI Server Startup Script
Runs FastAPI server with WebSocket + Vision endpoints
"""

import logging
import os
import signal
import sys

# Load environment variables
from dotenv import load_dotenv
load_dotenv()

import uvicorn

logging.basicConfig(
    level=getattr(logging, os.getenv("LOG_LEVEL", "INFO")),
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


def handle_shutdown(signum, frame):
    """Handle shutdown signals"""
    logger.info("Shutting down...")
    sys.exit(0)


def main():
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
    memory_backend = os.getenv("MEMORY_BACKEND", "qdrant")

    logger.info(f"Model: {model}")
    logger.info(f"Memory: {memory_backend}")

    host = os.getenv("SERVER_HOST", "0.0.0.0")
    port = int(os.getenv("SERVER_PORT", "8765"))

    logger.info(f"WebSocket: ws://{host}:{port}/ws")
    logger.info(f"Health: http://{host}:{port}/health")

    # Run FastAPI with uvicorn
    uvicorn.run(
        "main:app",
        host=host,
        port=port,
        reload=False,
        log_level="info",
        ws_ping_interval=30,
        ws_ping_timeout=10
    )


if __name__ == "__main__":
    signal.signal(signal.SIGINT, handle_shutdown)
    signal.signal(signal.SIGTERM, handle_shutdown)

    try:
        main()
    except KeyboardInterrupt:
        logger.info("Shutdown")
    except Exception as e:
        logger.error(f"Fatal: {e}")
        sys.exit(1)
