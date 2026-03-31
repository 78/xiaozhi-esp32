"""
OpenClaw HTTP API client: sends requests to OpenClaw's OpenAI-compatible endpoint.
"""

import json
import logging
from aiohttp import ClientSession

log = logging.getLogger(__name__)


class OpenClawClient:
    def __init__(self, cfg: dict):
        oc = cfg["openclaw"]
        self.base_url = oc["base_url"].rstrip("/")
        self.api_key = oc.get("api_key", "")
        self.agent_id = oc.get("agent_id", "")
        self.session_key = oc.get("session_key", "xiaozhi-proxy")

    async def chat(self, user_message: str) -> str:
        """Send a message to OpenClaw and return the assistant's text reply."""
        url = f"{self.base_url}/v1/chat/completions"
        headers = {
            "Content-Type": "application/json",
            "x-openclaw-session-key": self.session_key,
        }
        if self.api_key:
            headers["Authorization"] = f"Bearer {self.api_key}"

        payload = {
            "model": self.agent_id or "default",
            "messages": [{"role": "user", "content": user_message}],
            "stream": False,
        }

        try:
            async with ClientSession() as session:
                async with session.post(url, headers=headers,
                                        json=payload, timeout=30) as resp:
                    if resp.status != 200:
                        text = await resp.text()
                        log.error("OpenClaw error %d: %s", resp.status, text[:200])
                        return f"OpenClaw 请求失败 (HTTP {resp.status})"
                    data = await resp.json()
        except Exception as e:
            log.error("OpenClaw request failed: %s", e)
            return f"OpenClaw 请求异常: {e}"

        try:
            return data["choices"][0]["message"]["content"]
        except (KeyError, IndexError):
            log.error("Unexpected OpenClaw response: %s", json.dumps(data)[:300])
            return "OpenClaw 返回格式异常"

    async def set_reminder(self, time_str: str, message: str) -> str:
        """Set a reminder via OpenClaw's cron/message tools."""
        prompt = (
            f"请帮我设置一个定时提醒：在 {time_str} 提醒我 \"{message}\"。"
            f"请使用 cron 工具来创建这个定时任务，到时间后调用 webhook 通知。"
        )
        return await self.chat(prompt)

    async def web_search(self, query: str) -> str:
        prompt = f"请帮我联网搜索以下内容，给出简要摘要：{query}"
        return await self.chat(prompt)

    async def send_to_wechat(self, content: str) -> str:
        prompt = f"请把以下内容发送到我的微信：\n\n{content}"
        return await self.chat(prompt)
