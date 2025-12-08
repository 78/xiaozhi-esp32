#!/usr/bin/env python3
"""
Vector Memory Store for Spark AI
Uses ChromaDB for semantic search instead of keyword matching
"""

import os
import logging
import hashlib
from datetime import datetime
from typing import Optional, List, Dict, Any

logger = logging.getLogger(__name__)

MEMORY_DIR = os.getenv("MEMORY_DIR", "/data/memories")


class MemoryStore:
    """
    ChromaDB-backed vector memory with semantic search.
    Falls back to simple storage if ChromaDB unavailable.
    """

    def __init__(self, device_id: str):
        self.device_id = device_id
        self.collection_name = f"spark_{hashlib.md5(device_id.encode()).hexdigest()[:12]}"
        self.client = None
        self.collection = None
        self._init_store()

    def _init_store(self):
        """Initialize ChromaDB or fallback."""
        try:
            import chromadb
            from chromadb.config import Settings

            # Persistent storage
            os.makedirs(MEMORY_DIR, exist_ok=True)

            self.client = chromadb.PersistentClient(
                path=MEMORY_DIR,
                settings=Settings(anonymized_telemetry=False)
            )

            self.collection = self.client.get_or_create_collection(
                name=self.collection_name,
                metadata={"device_id": self.device_id}
            )

            logger.info(f"ChromaDB initialized for device: {self.device_id}")

        except ImportError:
            logger.warning("ChromaDB not installed, memory disabled")
        except Exception as e:
            logger.error(f"ChromaDB init failed: {e}")

    async def store(self, content: str, category: str = "conversation") -> bool:
        """Store a memory with automatic embedding."""
        if not self.collection:
            return False

        try:
            memory_id = hashlib.md5(
                f"{content}{datetime.now().isoformat()}".encode()
            ).hexdigest()[:16]

            self.collection.add(
                documents=[content],
                metadatas=[{
                    "category": category,
                    "timestamp": datetime.now().isoformat(),
                    "device_id": self.device_id
                }],
                ids=[memory_id]
            )

            logger.debug(f"Stored memory: {content[:50]}...")
            return True

        except Exception as e:
            logger.error(f"Memory store failed: {e}")
            return False

    async def recall(self, query: str, limit: int = 3) -> Optional[str]:
        """Recall relevant memories using semantic search."""
        if not self.collection:
            return None

        try:
            results = self.collection.query(
                query_texts=[query],
                n_results=limit
            )

            if not results or not results.get("documents"):
                return None

            documents = results["documents"][0]
            metadatas = results.get("metadatas", [[]])[0]

            if not documents:
                return None

            # Format results
            formatted = []
            for i, (doc, meta) in enumerate(zip(documents, metadatas)):
                timestamp = meta.get("timestamp", "")[:10] if meta else ""
                formatted.append(f"[{timestamp}] {doc}")

            return "\n".join(formatted)

        except Exception as e:
            logger.error(f"Memory recall failed: {e}")
            return None

    async def store_conversation(self, role: str, content: str):
        """Store a conversation turn."""
        await self.store(
            f"{role}: {content}",
            category="conversation"
        )

    async def store_fact(self, fact: str):
        """Store an explicit fact/preference."""
        await self.store(fact, category="fact")

    async def get_context(self, query: str, max_tokens: int = 500) -> str:
        """Get relevant context for the current query."""
        memories = await self.recall(query, limit=5)
        if not memories:
            return ""

        # Truncate if too long
        if len(memories) > max_tokens * 4:  # Rough char estimate
            memories = memories[:max_tokens * 4] + "..."

        return f"Relevant memories:\n{memories}"

    def clear(self):
        """Clear all memories for this device."""
        if self.client and self.collection:
            try:
                self.client.delete_collection(self.collection_name)
                self.collection = self.client.get_or_create_collection(
                    name=self.collection_name
                )
                logger.info(f"Cleared memories for device: {self.device_id}")
            except Exception as e:
                logger.error(f"Clear failed: {e}")


class MemoryManager:
    """Manages memory stores for multiple devices."""

    _stores: Dict[str, MemoryStore] = {}

    @classmethod
    def get_store(cls, device_id: str) -> MemoryStore:
        """Get or create memory store for device."""
        if device_id not in cls._stores:
            cls._stores[device_id] = MemoryStore(device_id)
        return cls._stores[device_id]
