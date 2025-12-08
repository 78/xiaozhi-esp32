#!/usr/bin/env python3
"""
Vision API Server for AI Watcher Camera
Handles image uploads and analysis via Gemini
"""

import os
import logging
import asyncio
import threading
from aiohttp import web

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Image ready event for camera flow coordination
image_ready_event = asyncio.Event()

# Path for latest captured image (use /dev/shm for speed)
LATEST_IMAGE_PATH = os.getenv("IMAGE_PATH", "/dev/shm/spark_latest.jpg")


class VisionServer:
    """HTTP server for vision/camera API"""

    def __init__(self):
        self.gemini = None
        self._init_gemini()

    def _init_gemini(self):
        """Initialize Gemini client if available."""
        try:
            from gemini_client import GeminiClient
            self.gemini = GeminiClient()
        except ImportError:
            logger.warning("GeminiClient not available, vision analysis disabled")

    async def handle_vision(self, request: web.Request) -> web.Response:
        """Handle image analysis request from device"""

        try:
            # Check content type
            content_type = request.headers.get("Content-Type", "")

            if "multipart/form-data" in content_type:
                # Handle multipart form upload
                reader = await request.multipart()

                image_data = None
                question = "What do you see in this image? Describe it briefly."

                async for part in reader:
                    if part.name == "file" or part.name == "image":
                        image_data = await part.read()
                    elif part.name == "question":
                        question = (await part.read()).decode()

                if not image_data:
                    return web.json_response(
                        {"error": "No image provided"},
                        status=400
                    )

            elif "application/octet-stream" in content_type or "image/" in content_type:
                # Raw image data
                image_data = await request.read()
                question = request.query.get("question", "What do you see in this image?")

            else:
                # JSON request with base64 image
                data = await request.json()
                import base64
                image_data = base64.b64decode(data.get("image", ""))
                question = data.get("question", "What do you see in this image?")

            if not image_data:
                return web.json_response(
                    {"error": "No image data received"},
                    status=400
                )

            logger.info(f"Analyzing image, size: {len(image_data)} bytes, question: {question}")

            # Save image to fast path for Gemini Live to pick up
            try:
                with open(LATEST_IMAGE_PATH, 'wb') as f:
                    f.write(image_data)
                logger.info(f"Saved image to {LATEST_IMAGE_PATH}")

                # Signal that image is ready
                image_ready_event.set()
            except Exception as e:
                logger.error(f"Failed to save image: {e}")

            # Analyze with Gemini if available
            if self.gemini:
                result = await self.gemini.analyze_image(image_data, question)
            else:
                result = "Image received, analysis pending via Gemini Live"

            return web.json_response({
                "success": True,
                "description": result
            })

        except Exception as e:
            logger.error(f"Vision analysis error: {e}", exc_info=True)
            return web.json_response(
                {"error": str(e)},
                status=500
            )

    async def handle_upload(self, request: web.Request) -> web.Response:
        """Handle raw image upload (for MCP camera flow)"""
        try:
            image_data = await request.read()

            if not image_data:
                return web.json_response({"error": "No data"}, status=400)

            # Save image
            with open(LATEST_IMAGE_PATH, 'wb') as f:
                f.write(image_data)

            logger.info(f"Image uploaded: {len(image_data)} bytes")

            # Signal ready
            image_ready_event.set()

            return web.json_response({"success": True, "size": len(image_data)})

        except Exception as e:
            logger.error(f"Upload error: {e}")
            return web.json_response({"error": str(e)}, status=500)

    async def handle_health(self, request: web.Request) -> web.Response:
        """Health check endpoint"""
        return web.json_response({"status": "healthy", "service": "spark-vision"})


async def create_vision_app() -> web.Application:
    """Create the vision server application"""
    server = VisionServer()

    app = web.Application(client_max_size=10 * 1024 * 1024)  # 10MB max
    app.router.add_post("/vision", server.handle_vision)
    app.router.add_post("/api/vision", server.handle_vision)
    app.router.add_post("/upload", server.handle_upload)
    app.router.add_get("/health", server.handle_health)
    app.router.add_get("/", server.handle_health)

    return app


def start_vision_server():
    """Start vision server in a new event loop (for threading)"""
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    async def run():
        app = await create_vision_app()

        host = os.getenv("VISION_HOST", "0.0.0.0")
        port = int(os.getenv("VISION_PORT", "8766"))

        runner = web.AppRunner(app)
        await runner.setup()

        site = web.TCPSite(runner, host, port)
        await site.start()

        logger.info(f"Vision server running on http://{host}:{port}")

        # Run forever
        await asyncio.Event().wait()

    loop.run_until_complete(run())


async def run_vision_server():
    """Run the vision server (async version)"""
    app = await create_vision_app()

    host = os.getenv("VISION_HOST", "0.0.0.0")
    port = int(os.getenv("VISION_PORT", "8766"))

    runner = web.AppRunner(app)
    await runner.setup()

    site = web.TCPSite(runner, host, port)
    await site.start()

    logger.info(f"Vision server running on {host}:{port}")

    # Run forever
    await asyncio.Event().wait()


if __name__ == "__main__":
    asyncio.run(run_vision_server())
