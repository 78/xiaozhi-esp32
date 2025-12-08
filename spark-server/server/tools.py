#!/usr/bin/env python3
"""
Tools for Spark AI - Web Search and Memory
"""

import os
import logging
import json
import asyncio
from typing import Optional, List, Dict, Any
from datetime import datetime
import aiohttp
import hashlib

logger = logging.getLogger(__name__)


class WebSearchTool:
    """Google Search tool using Custom Search API or SerpAPI"""

    def __init__(self):
        self.google_api_key = os.getenv("GOOGLE_API_KEY", "")
        self.google_cx = os.getenv("GOOGLE_SEARCH_CX", "")
        self.serpapi_key = os.getenv("SERPAPI_KEY", "")
        self.session: Optional[aiohttp.ClientSession] = None

    async def get_session(self) -> aiohttp.ClientSession:
        if self.session is None or self.session.closed:
            self.session = aiohttp.ClientSession()
        return self.session

    async def search(self, query: str, num_results: int = 5) -> str:
        """Perform web search and return formatted results"""

        # Try Google Custom Search first
        if self.google_api_key and self.google_cx:
            return await self._google_search(query, num_results)

        # Fall back to SerpAPI
        if self.serpapi_key:
            return await self._serpapi_search(query, num_results)

        # Fall back to DuckDuckGo (no API key needed)
        return await self._duckduckgo_search(query, num_results)

    async def _google_search(self, query: str, num_results: int) -> str:
        """Search using Google Custom Search API"""
        session = await self.get_session()

        url = "https://www.googleapis.com/customsearch/v1"
        params = {
            "key": self.google_api_key,
            "cx": self.google_cx,
            "q": query,
            "num": num_results
        }

        try:
            async with session.get(url, params=params) as response:
                if response.status == 200:
                    data = await response.json()
                    items = data.get("items", [])
                    return self._format_results(items, "google")
                else:
                    logger.error(f"Google search error: {response.status}")
        except Exception as e:
            logger.error(f"Google search failed: {e}")

        return "Search unavailable at the moment."

    async def _serpapi_search(self, query: str, num_results: int) -> str:
        """Search using SerpAPI"""
        session = await self.get_session()

        url = "https://serpapi.com/search"
        params = {
            "api_key": self.serpapi_key,
            "engine": "google",
            "q": query,
            "num": num_results
        }

        try:
            async with session.get(url, params=params) as response:
                if response.status == 200:
                    data = await response.json()
                    results = data.get("organic_results", [])
                    return self._format_results(results, "serpapi")
        except Exception as e:
            logger.error(f"SerpAPI search failed: {e}")

        return "Search unavailable at the moment."

    async def _duckduckgo_search(self, query: str, num_results: int) -> str:
        """Search using DuckDuckGo Instant Answer API (no API key needed)"""
        session = await self.get_session()

        # DuckDuckGo Instant Answer API
        url = "https://api.duckduckgo.com/"
        params = {
            "q": query,
            "format": "json",
            "no_html": 1,
            "skip_disambig": 1
        }

        try:
            async with session.get(url, params=params) as response:
                if response.status == 200:
                    data = await response.json()

                    results = []

                    # Abstract (main result)
                    if data.get("Abstract"):
                        results.append({
                            "title": data.get("Heading", "Result"),
                            "snippet": data.get("Abstract"),
                            "url": data.get("AbstractURL", "")
                        })

                    # Related topics
                    for topic in data.get("RelatedTopics", [])[:num_results-1]:
                        if isinstance(topic, dict) and "Text" in topic:
                            results.append({
                                "title": topic.get("Text", "")[:50],
                                "snippet": topic.get("Text", ""),
                                "url": topic.get("FirstURL", "")
                            })

                    if results:
                        return self._format_results(results, "duckduckgo")

        except Exception as e:
            logger.error(f"DuckDuckGo search failed: {e}")

        return "No search results found."

    def _format_results(self, results: List[Dict], source: str) -> str:
        """Format search results for the AI"""
        formatted = []

        for i, result in enumerate(results[:5], 1):
            if source == "google":
                title = result.get("title", "")
                snippet = result.get("snippet", "")
                url = result.get("link", "")
            elif source == "serpapi":
                title = result.get("title", "")
                snippet = result.get("snippet", "")
                url = result.get("link", "")
            else:  # duckduckgo
                title = result.get("title", "")
                snippet = result.get("snippet", "")
                url = result.get("url", "")

            formatted.append(f"{i}. {title}\n   {snippet}\n   Source: {url}")

        return "\n\n".join(formatted) if formatted else "No results found."


class MemoryTool:
    """Simple file-based memory system with semantic-like recall"""

    def __init__(self):
        self.memory_dir = os.getenv("MEMORY_DIR", "/data/memories")
        os.makedirs(self.memory_dir, exist_ok=True)

    def _get_device_memory_file(self, device_id: str) -> str:
        """Get the memory file path for a device"""
        safe_id = hashlib.md5(device_id.encode()).hexdigest()[:16]
        return os.path.join(self.memory_dir, f"memory_{safe_id}.json")

    def _load_memories(self, device_id: str) -> List[Dict[str, Any]]:
        """Load memories for a device"""
        filepath = self._get_device_memory_file(device_id)
        if os.path.exists(filepath):
            try:
                with open(filepath, "r") as f:
                    return json.load(f)
            except Exception as e:
                logger.error(f"Failed to load memories: {e}")
        return []

    def _save_memories(self, device_id: str, memories: List[Dict[str, Any]]):
        """Save memories for a device"""
        filepath = self._get_device_memory_file(device_id)
        try:
            with open(filepath, "w") as f:
                json.dump(memories, f, indent=2)
        except Exception as e:
            logger.error(f"Failed to save memories: {e}")

    async def store(self, content: str, device_id: str, category: str = "general") -> bool:
        """Store a memory"""
        memories = self._load_memories(device_id)

        memory = {
            "id": hashlib.md5(f"{content}{datetime.now().isoformat()}".encode()).hexdigest()[:12],
            "content": content,
            "category": category,
            "timestamp": datetime.now().isoformat(),
            "keywords": self._extract_keywords(content)
        }

        memories.append(memory)

        # Keep only last 1000 memories
        if len(memories) > 1000:
            memories = memories[-1000:]

        self._save_memories(device_id, memories)
        logger.info(f"Stored memory: {content[:50]}...")
        return True

    async def recall(self, query: str, device_id: str, limit: int = 5) -> Optional[str]:
        """Recall relevant memories"""
        memories = self._load_memories(device_id)

        if not memories:
            return None

        # Simple keyword matching for recall
        query_keywords = set(self._extract_keywords(query))

        scored_memories = []
        for memory in memories:
            memory_keywords = set(memory.get("keywords", []))
            overlap = len(query_keywords & memory_keywords)
            if overlap > 0:
                scored_memories.append((overlap, memory))

        # Sort by relevance
        scored_memories.sort(key=lambda x: x[0], reverse=True)

        if not scored_memories:
            return None

        # Format results
        results = []
        for score, memory in scored_memories[:limit]:
            timestamp = memory.get("timestamp", "")[:10]  # Just date
            content = memory.get("content", "")
            results.append(f"[{timestamp}] {content}")

        return "\n".join(results)

    async def list_recent(self, device_id: str, limit: int = 10) -> List[Dict]:
        """List recent memories"""
        memories = self._load_memories(device_id)
        return memories[-limit:]

    async def clear(self, device_id: str) -> bool:
        """Clear all memories for a device"""
        filepath = self._get_device_memory_file(device_id)
        if os.path.exists(filepath):
            os.remove(filepath)
        return True

    def _extract_keywords(self, text: str) -> List[str]:
        """Extract keywords from text (simple implementation)"""
        # Remove common words and punctuation
        stopwords = {
            "the", "a", "an", "is", "are", "was", "were", "be", "been",
            "being", "have", "has", "had", "do", "does", "did", "will",
            "would", "could", "should", "may", "might", "must", "shall",
            "can", "need", "dare", "ought", "used", "to", "of", "in",
            "for", "on", "with", "at", "by", "from", "as", "into",
            "through", "during", "before", "after", "above", "below",
            "between", "under", "again", "further", "then", "once",
            "here", "there", "when", "where", "why", "how", "all",
            "each", "few", "more", "most", "other", "some", "such",
            "no", "nor", "not", "only", "own", "same", "so", "than",
            "too", "very", "just", "and", "but", "if", "or", "because",
            "until", "while", "this", "that", "these", "those", "i",
            "me", "my", "myself", "we", "our", "ours", "ourselves",
            "you", "your", "yours", "yourself", "yourselves", "he",
            "him", "his", "himself", "she", "her", "hers", "herself",
            "it", "its", "itself", "they", "them", "their", "theirs",
            "themselves", "what", "which", "who", "whom", "remember",
            "don't", "forget", "note", "save", "please"
        }

        # Simple tokenization
        import re
        words = re.findall(r'\b[a-zA-Z]+\b', text.lower())

        # Filter and return
        keywords = [w for w in words if w not in stopwords and len(w) > 2]
        return list(set(keywords))[:20]  # Max 20 keywords
