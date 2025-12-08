#!/usr/bin/env python3
"""
Gemini 2.5 Flash Multimodal Client
Handles text, audio, and vision processing
"""

import os
import logging
import base64
from typing import Optional, List, Dict, Any
import asyncio
import aiohttp
import json

logger = logging.getLogger(__name__)

# Gemini API configuration
GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "")
GEMINI_MODEL = os.getenv("GEMINI_MODEL", "gemini-2.0-flash-exp")  # Latest multimodal
GEMINI_BASE_URL = "https://generativelanguage.googleapis.com/v1beta"


class GeminiClient:
    """Client for Gemini 2.5 multimodal API"""

    def __init__(self):
        self.api_key = GEMINI_API_KEY
        if not self.api_key:
            logger.warning("GEMINI_API_KEY not set!")
        self.model = GEMINI_MODEL
        self.session: Optional[aiohttp.ClientSession] = None

    async def get_session(self) -> aiohttp.ClientSession:
        """Get or create aiohttp session"""
        if self.session is None or self.session.closed:
            self.session = aiohttp.ClientSession()
        return self.session

    async def generate_response(
        self,
        system_prompt: str,
        conversation: List[Dict[str, Any]],
        user_input: str,
        context: str = "",
        image_data: Optional[bytes] = None
    ) -> str:
        """Generate a response using Gemini"""

        session = await self.get_session()

        # Build the contents array
        contents = []

        # Add conversation history (last 10 messages for context)
        for msg in conversation[-10:]:
            role = "user" if msg["role"] == "user" else "model"
            parts = [{"text": msg["content"]}]

            # Include image if present
            if "image" in msg:
                parts.append({
                    "inline_data": {
                        "mime_type": "image/jpeg",
                        "data": msg["image"]
                    }
                })

            contents.append({"role": role, "parts": parts})

        # Build current message parts
        current_parts = []

        # Add context from tools if available
        if context:
            current_parts.append({"text": f"[Context: {context}]\n\n{user_input}"})
        else:
            current_parts.append({"text": user_input})

        # Add image if provided
        if image_data:
            current_parts.append({
                "inline_data": {
                    "mime_type": "image/jpeg",
                    "data": base64.b64encode(image_data).decode()
                }
            })

        contents.append({"role": "user", "parts": current_parts})

        # Build request
        url = f"{GEMINI_BASE_URL}/models/{self.model}:generateContent?key={self.api_key}"

        payload = {
            "contents": contents,
            "systemInstruction": {
                "parts": [{"text": system_prompt}]
            },
            "generationConfig": {
                "temperature": 0.9,
                "topK": 40,
                "topP": 0.95,
                "maxOutputTokens": 500,  # Keep responses concise for TTS
                "responseMimeType": "text/plain"
            },
            "safetySettings": [
                {"category": "HARM_CATEGORY_HARASSMENT", "threshold": "BLOCK_ONLY_HIGH"},
                {"category": "HARM_CATEGORY_HATE_SPEECH", "threshold": "BLOCK_ONLY_HIGH"},
                {"category": "HARM_CATEGORY_SEXUALLY_EXPLICIT", "threshold": "BLOCK_ONLY_HIGH"},
                {"category": "HARM_CATEGORY_DANGEROUS_CONTENT", "threshold": "BLOCK_ONLY_HIGH"}
            ]
        }

        try:
            async with session.post(url, json=payload) as response:
                if response.status == 200:
                    data = await response.json()
                    candidates = data.get("candidates", [])
                    if candidates:
                        content = candidates[0].get("content", {})
                        parts = content.get("parts", [])
                        if parts:
                            return parts[0].get("text", "")
                    return "Hmm, I'm drawing a blank here. Mind running that by me again?"
                else:
                    error = await response.text()
                    logger.error(f"Gemini API error: {response.status} - {error}")
                    return "Sorry, having a bit of technical difficulty. Let's try again in a mo."

        except Exception as e:
            logger.error(f"Gemini request failed: {e}")
            return "Blimey, something went wrong on my end. Give us another shot?"

    async def transcribe_audio(self, audio_data: bytes) -> Optional[str]:
        """Transcribe audio using Gemini's multimodal capabilities"""

        session = await self.get_session()

        # Gemini can process audio directly
        url = f"{GEMINI_BASE_URL}/models/{self.model}:generateContent?key={self.api_key}"

        payload = {
            "contents": [{
                "role": "user",
                "parts": [
                    {
                        "inline_data": {
                            "mime_type": "audio/opus",
                            "data": base64.b64encode(audio_data).decode()
                        }
                    },
                    {"text": "Transcribe this audio exactly as spoken. Only output the transcription, nothing else."}
                ]
            }],
            "generationConfig": {
                "temperature": 0.1,
                "maxOutputTokens": 500
            }
        }

        try:
            async with session.post(url, json=payload) as response:
                if response.status == 200:
                    data = await response.json()
                    candidates = data.get("candidates", [])
                    if candidates:
                        parts = candidates[0].get("content", {}).get("parts", [])
                        if parts:
                            return parts[0].get("text", "").strip()
                else:
                    error = await response.text()
                    logger.error(f"Transcription error: {response.status} - {error}")

        except Exception as e:
            logger.error(f"Transcription failed: {e}")

        return None

    async def analyze_image(self, image_data: bytes, question: str) -> str:
        """Analyze an image using Gemini vision"""

        session = await self.get_session()

        url = f"{GEMINI_BASE_URL}/models/{self.model}:generateContent?key={self.api_key}"

        payload = {
            "contents": [{
                "role": "user",
                "parts": [
                    {
                        "inline_data": {
                            "mime_type": "image/jpeg",
                            "data": base64.b64encode(image_data).decode()
                        }
                    },
                    {"text": question}
                ]
            }],
            "systemInstruction": {
                "parts": [{
                    "text": "You are Spark, analyzing images for the user. Be concise but descriptive. Use British English and a friendly tone."
                }]
            },
            "generationConfig": {
                "temperature": 0.7,
                "maxOutputTokens": 300
            }
        }

        try:
            async with session.post(url, json=payload) as response:
                if response.status == 200:
                    data = await response.json()
                    candidates = data.get("candidates", [])
                    if candidates:
                        parts = candidates[0].get("content", {}).get("parts", [])
                        if parts:
                            return parts[0].get("text", "")
                else:
                    error = await response.text()
                    logger.error(f"Image analysis error: {response.status} - {error}")

        except Exception as e:
            logger.error(f"Image analysis failed: {e}")

        return "Sorry, couldn't quite make out what's in that image."

    async def text_to_speech(self, text: str) -> Optional[bytes]:
        """Convert text to speech using edge-tts (free, no API key needed)

        Available voices (natural, not robotic):
        - en-GB-LibbyNeural: British female, young, casual, friendly
        - en-GB-MaisieNeural: British female, warm, conversational
        - en-US-JennyNeural: American female, friendly, natural
        - en-US-AriaNeural: American female, warm, expressive
        """
        try:
            import edge_tts

            # Get voice from environment or use default
            # LibbyNeural is young British, casual and friendly - not posh!
            voice = os.getenv("TTS_VOICE", "en-GB-LibbyNeural")
            rate = os.getenv("TTS_RATE", "+5%")  # Slightly faster, natural pace

            communicate = edge_tts.Communicate(
                text,
                voice=voice,
                rate=rate
            )

            audio_data = b""
            async for chunk in communicate.stream():
                if chunk["type"] == "audio":
                    audio_data += chunk["data"]

            if audio_data:
                # Convert to opus format for the device
                return await self._convert_to_opus(audio_data)

        except ImportError:
            logger.warning("edge-tts not installed, TTS disabled")
        except Exception as e:
            logger.error(f"TTS failed: {e}")

        return None

    async def _convert_to_opus(self, audio_data: bytes) -> bytes:
        """Convert audio to opus format for the device"""
        try:
            import subprocess
            import tempfile

            with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as f:
                f.write(audio_data)
                input_file = f.name

            output_file = input_file.replace(".mp3", ".opus")

            # Use ffmpeg to convert
            process = await asyncio.create_subprocess_exec(
                "ffmpeg", "-i", input_file,
                "-c:a", "libopus",
                "-b:a", "24k",
                "-ar", "24000",
                "-ac", "1",
                "-y", output_file,
                stdout=asyncio.subprocess.DEVNULL,
                stderr=asyncio.subprocess.DEVNULL
            )
            await process.wait()

            with open(output_file, "rb") as f:
                opus_data = f.read()

            # Cleanup
            os.unlink(input_file)
            os.unlink(output_file)

            return opus_data

        except Exception as e:
            logger.error(f"Audio conversion failed: {e}")
            return audio_data  # Return original if conversion fails

    async def close(self):
        """Close the client session"""
        if self.session and not self.session.closed:
            await self.session.close()
