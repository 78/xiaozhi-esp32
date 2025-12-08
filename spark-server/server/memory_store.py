#!/usr/bin/env python3
"""
Memory Store for Spark AI
Pluggable backends: Qdrant, Redis, Supabase, ChromaDB

Syncs device memories to your main AI memory system.
"""

import os
import logging
import hashlib
import json
from abc import ABC, abstractmethod
from datetime import datetime
from typing import Optional, List, Dict, Any

logger = logging.getLogger(__name__)

# Configuration
MEMORY_BACKEND = os.getenv("MEMORY_BACKEND", "qdrant")  # qdrant, redis, supabase, chromadb
MEMORY_DIR = os.getenv("MEMORY_DIR", "/data/memories")

# Qdrant
QDRANT_HOST = os.getenv("QDRANT_HOST", "localhost")
QDRANT_PORT = int(os.getenv("QDRANT_PORT", "6333"))
QDRANT_API_KEY = os.getenv("QDRANT_API_KEY", "")
QDRANT_COLLECTION = os.getenv("QDRANT_COLLECTION", "spark_memories")

# Redis
REDIS_URL = os.getenv("REDIS_URL", "redis://localhost:6379")
REDIS_PREFIX = os.getenv("REDIS_PREFIX", "spark:memory:")

# Supabase
SUPABASE_URL = os.getenv("SUPABASE_URL", "")
SUPABASE_KEY = os.getenv("SUPABASE_KEY", "")
SUPABASE_TABLE = os.getenv("SUPABASE_TABLE", "ai_memories")


class MemoryBackend(ABC):
    """Abstract base for memory backends."""

    @abstractmethod
    async def store(self, device_id: str, content: str, metadata: Dict) -> bool:
        pass

    @abstractmethod
    async def recall(self, device_id: str, query: str, limit: int) -> List[Dict]:
        pass

    @abstractmethod
    async def sync_to_main(self, device_id: str, memory: Dict) -> bool:
        """Sync memory to main AI system."""
        pass


class QdrantBackend(MemoryBackend):
    """Qdrant vector database backend."""

    def __init__(self):
        self.client = None
        self._init_client()

    def _init_client(self):
        try:
            from qdrant_client import QdrantClient
            from qdrant_client.models import Distance, VectorParams

            if QDRANT_API_KEY:
                self.client = QdrantClient(
                    host=QDRANT_HOST,
                    port=QDRANT_PORT,
                    api_key=QDRANT_API_KEY
                )
            else:
                self.client = QdrantClient(host=QDRANT_HOST, port=QDRANT_PORT)

            # Ensure collection exists (384 dim for default sentence transformer)
            collections = [c.name for c in self.client.get_collections().collections]
            if QDRANT_COLLECTION not in collections:
                self.client.create_collection(
                    collection_name=QDRANT_COLLECTION,
                    vectors_config=VectorParams(size=384, distance=Distance.COSINE)
                )

            logger.info(f"Qdrant connected: {QDRANT_HOST}:{QDRANT_PORT}")

        except ImportError:
            logger.error("qdrant-client not installed")
        except Exception as e:
            logger.error(f"Qdrant init failed: {e}")

    def _get_embedding(self, text: str) -> List[float]:
        """Get embedding vector for text."""
        try:
            from sentence_transformers import SentenceTransformer
            model = SentenceTransformer('all-MiniLM-L6-v2')
            return model.encode(text).tolist()
        except ImportError:
            # Fallback: simple hash-based pseudo-embedding
            import hashlib
            h = hashlib.sha384(text.encode()).digest()
            return [float(b) / 255.0 for b in h]

    async def store(self, device_id: str, content: str, metadata: Dict) -> bool:
        if not self.client:
            return False

        try:
            from qdrant_client.models import PointStruct

            point_id = hashlib.md5(
                f"{device_id}{content}{datetime.now().isoformat()}".encode()
            ).hexdigest()

            vector = self._get_embedding(content)

            self.client.upsert(
                collection_name=QDRANT_COLLECTION,
                points=[PointStruct(
                    id=point_id,
                    vector=vector,
                    payload={
                        "device_id": device_id,
                        "content": content,
                        "source": "spark_watcher",
                        **metadata
                    }
                )]
            )
            return True

        except Exception as e:
            logger.error(f"Qdrant store failed: {e}")
            return False

    async def recall(self, device_id: str, query: str, limit: int) -> List[Dict]:
        if not self.client:
            return []

        try:
            from qdrant_client.models import Filter, FieldCondition, MatchValue

            vector = self._get_embedding(query)

            results = self.client.search(
                collection_name=QDRANT_COLLECTION,
                query_vector=vector,
                query_filter=Filter(
                    must=[FieldCondition(
                        key="device_id",
                        match=MatchValue(value=device_id)
                    )]
                ),
                limit=limit
            )

            return [
                {
                    "content": r.payload.get("content", ""),
                    "score": r.score,
                    "metadata": r.payload
                }
                for r in results
            ]

        except Exception as e:
            logger.error(f"Qdrant recall failed: {e}")
            return []

    async def sync_to_main(self, device_id: str, memory: Dict) -> bool:
        # Qdrant is already the main system - memories go directly there
        return True


class RedisBackend(MemoryBackend):
    """Redis backend with JSON storage + optional vector search."""

    def __init__(self):
        self.client = None
        self._init_client()

    def _init_client(self):
        try:
            import redis

            self.client = redis.from_url(REDIS_URL, decode_responses=True)
            self.client.ping()
            logger.info(f"Redis connected: {REDIS_URL}")

        except ImportError:
            logger.error("redis not installed")
        except Exception as e:
            logger.error(f"Redis init failed: {e}")

    async def store(self, device_id: str, content: str, metadata: Dict) -> bool:
        if not self.client:
            return False

        try:
            memory_id = hashlib.md5(
                f"{content}{datetime.now().isoformat()}".encode()
            ).hexdigest()[:16]

            key = f"{REDIS_PREFIX}{device_id}:{memory_id}"

            data = {
                "content": content,
                "device_id": device_id,
                "source": "spark_watcher",
                "timestamp": datetime.now().isoformat(),
                **metadata
            }

            self.client.set(key, json.dumps(data))
            # Add to device's memory list
            self.client.lpush(f"{REDIS_PREFIX}{device_id}:list", memory_id)
            # Keep last 1000 memories
            self.client.ltrim(f"{REDIS_PREFIX}{device_id}:list", 0, 999)

            return True

        except Exception as e:
            logger.error(f"Redis store failed: {e}")
            return False

    async def recall(self, device_id: str, query: str, limit: int) -> List[Dict]:
        if not self.client:
            return []

        try:
            # Get recent memory IDs
            memory_ids = self.client.lrange(
                f"{REDIS_PREFIX}{device_id}:list", 0, limit * 3
            )

            results = []
            query_lower = query.lower()

            for mid in memory_ids:
                key = f"{REDIS_PREFIX}{device_id}:{mid}"
                data = self.client.get(key)
                if data:
                    memory = json.loads(data)
                    content = memory.get("content", "").lower()
                    # Simple relevance scoring
                    score = sum(1 for word in query_lower.split() if word in content)
                    if score > 0:
                        results.append({
                            "content": memory.get("content"),
                            "score": score,
                            "metadata": memory
                        })

            # Sort by score and return top results
            results.sort(key=lambda x: x["score"], reverse=True)
            return results[:limit]

        except Exception as e:
            logger.error(f"Redis recall failed: {e}")
            return []

    async def sync_to_main(self, device_id: str, memory: Dict) -> bool:
        # Publish to Redis channel for main system to pick up
        if self.client:
            try:
                self.client.publish(
                    "spark:memory:sync",
                    json.dumps({"device_id": device_id, "memory": memory})
                )
                return True
            except:
                pass
        return False


class SupabaseBackend(MemoryBackend):
    """Supabase PostgreSQL backend with pgvector support."""

    def __init__(self):
        self.client = None
        self._init_client()

    def _init_client(self):
        if not SUPABASE_URL or not SUPABASE_KEY:
            logger.warning("Supabase credentials not configured")
            return

        try:
            from supabase import create_client

            self.client = create_client(SUPABASE_URL, SUPABASE_KEY)
            logger.info("Supabase connected")

        except ImportError:
            logger.error("supabase not installed")
        except Exception as e:
            logger.error(f"Supabase init failed: {e}")

    async def store(self, device_id: str, content: str, metadata: Dict) -> bool:
        if not self.client:
            return False

        try:
            data = {
                "device_id": device_id,
                "content": content,
                "source": "spark_watcher",
                "metadata": metadata,
                "created_at": datetime.now().isoformat()
            }

            self.client.table(SUPABASE_TABLE).insert(data).execute()
            return True

        except Exception as e:
            logger.error(f"Supabase store failed: {e}")
            return False

    async def recall(self, device_id: str, query: str, limit: int) -> List[Dict]:
        if not self.client:
            return []

        try:
            # Text search (or use pgvector if configured)
            response = self.client.table(SUPABASE_TABLE)\
                .select("*")\
                .eq("device_id", device_id)\
                .ilike("content", f"%{query}%")\
                .order("created_at", desc=True)\
                .limit(limit)\
                .execute()

            return [
                {
                    "content": r.get("content"),
                    "score": 1.0,
                    "metadata": r
                }
                for r in response.data
            ]

        except Exception as e:
            logger.error(f"Supabase recall failed: {e}")
            return []

    async def sync_to_main(self, device_id: str, memory: Dict) -> bool:
        # Already in Supabase - main system can query directly
        return True


class ChromaDBBackend(MemoryBackend):
    """ChromaDB local backend (fallback)."""

    def __init__(self):
        self.client = None
        self.collections: Dict[str, Any] = {}
        self._init_client()

    def _init_client(self):
        try:
            import chromadb
            from chromadb.config import Settings

            os.makedirs(MEMORY_DIR, exist_ok=True)

            self.client = chromadb.PersistentClient(
                path=MEMORY_DIR,
                settings=Settings(anonymized_telemetry=False)
            )
            logger.info("ChromaDB initialized")

        except ImportError:
            logger.warning("ChromaDB not installed")
        except Exception as e:
            logger.error(f"ChromaDB init failed: {e}")

    def _get_collection(self, device_id: str):
        if device_id not in self.collections:
            name = f"spark_{hashlib.md5(device_id.encode()).hexdigest()[:12]}"
            self.collections[device_id] = self.client.get_or_create_collection(name)
        return self.collections[device_id]

    async def store(self, device_id: str, content: str, metadata: Dict) -> bool:
        if not self.client:
            return False

        try:
            collection = self._get_collection(device_id)
            memory_id = hashlib.md5(
                f"{content}{datetime.now().isoformat()}".encode()
            ).hexdigest()[:16]

            collection.add(
                documents=[content],
                metadatas=[{"device_id": device_id, **metadata}],
                ids=[memory_id]
            )
            return True

        except Exception as e:
            logger.error(f"ChromaDB store failed: {e}")
            return False

    async def recall(self, device_id: str, query: str, limit: int) -> List[Dict]:
        if not self.client:
            return []

        try:
            collection = self._get_collection(device_id)
            results = collection.query(query_texts=[query], n_results=limit)

            if not results.get("documents"):
                return []

            return [
                {
                    "content": doc,
                    "score": 1.0 - (dist if dist else 0),
                    "metadata": meta
                }
                for doc, meta, dist in zip(
                    results["documents"][0],
                    results.get("metadatas", [[]])[0],
                    results.get("distances", [[]])[0]
                )
            ]

        except Exception as e:
            logger.error(f"ChromaDB recall failed: {e}")
            return []

    async def sync_to_main(self, device_id: str, memory: Dict) -> bool:
        # Local only - no sync
        return False


# =============================================================================
# Main Memory Store Interface
# =============================================================================

class MemoryStore:
    """
    Unified memory interface with pluggable backends.
    Automatically syncs to main AI system.
    """

    def __init__(self, device_id: str):
        self.device_id = device_id
        self.backend = self._create_backend()

    def _create_backend(self) -> MemoryBackend:
        """Create backend based on configuration."""
        backend_type = MEMORY_BACKEND.lower()

        if backend_type == "qdrant":
            return QdrantBackend()
        elif backend_type == "redis":
            return RedisBackend()
        elif backend_type == "supabase":
            return SupabaseBackend()
        else:
            return ChromaDBBackend()

    async def store(self, content: str, category: str = "conversation") -> bool:
        """Store a memory."""
        metadata = {
            "category": category,
            "timestamp": datetime.now().isoformat()
        }

        success = await self.backend.store(self.device_id, content, metadata)

        # Sync to main system
        if success:
            await self.backend.sync_to_main(self.device_id, {
                "content": content,
                **metadata
            })

        return success

    async def recall(self, query: str, limit: int = 3) -> Optional[str]:
        """Recall relevant memories."""
        results = await self.backend.recall(self.device_id, query, limit)

        if not results:
            return None

        formatted = []
        for r in results:
            timestamp = r.get("metadata", {}).get("timestamp", "")[:10]
            formatted.append(f"[{timestamp}] {r['content']}")

        return "\n".join(formatted)

    async def store_conversation(self, role: str, content: str):
        """Store conversation turn."""
        await self.store(f"{role}: {content}", category="conversation")

    async def store_fact(self, fact: str):
        """Store explicit fact."""
        await self.store(fact, category="fact")

    async def get_context(self, query: str, max_chars: int = 2000) -> str:
        """Get relevant context for query."""
        memories = await self.recall(query, limit=5)
        if not memories:
            return ""

        if len(memories) > max_chars:
            memories = memories[:max_chars] + "..."

        return f"Relevant memories:\n{memories}"


class MemoryManager:
    """
    Manages memory stores per device.
    Can be used as instance or class methods.
    """

    _stores: Dict[str, MemoryStore] = {}

    def __init__(self, device_id: str = "default"):
        """Initialize with a device ID."""
        self.device_id = device_id
        self._store = self.get_store(device_id)

    @classmethod
    def get_store(cls, device_id: str) -> MemoryStore:
        """Get or create memory store for device."""
        if device_id not in cls._stores:
            cls._stores[device_id] = MemoryStore(device_id)
            logger.info(f"Memory store created: {device_id} (backend: {MEMORY_BACKEND})")
        return cls._stores[device_id]

    async def get_system_context(self) -> str:
        """
        Get system context for cold start.
        Loads user profile and recent memories.
        """
        context_parts = []

        # Get user facts
        facts = await self._store.recall("user preferences facts profile", limit=5)
        if facts:
            context_parts.append(f"Known facts:\n{facts}")

        # Get recent conversation context
        recent = await self._store.recall("recent conversation context", limit=3)
        if recent:
            context_parts.append(f"Recent context:\n{recent}")

        return "\n\n".join(context_parts) if context_parts else ""

    async def store(self, content: str, metadata: Optional[Dict] = None) -> bool:
        """Store content with optional metadata."""
        category = metadata.get("type", "general") if metadata else "general"
        return await self._store.store(content, category=category)

    async def recall(self, query: str, limit: int = 5) -> List[Dict]:
        """Recall memories matching query."""
        return await self._store.backend.recall(self.device_id, query, limit)

    async def store_fact(self, fact: str) -> bool:
        """Store a fact about the user."""
        return await self._store.store_fact(fact)

    async def get_context(self, query: str) -> str:
        """Get context for a specific query."""
        return await self._store.get_context(query)
