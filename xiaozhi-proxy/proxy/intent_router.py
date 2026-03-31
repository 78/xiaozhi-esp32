"""
Intent Router: examines STT text from the official server and decides whether
to additionally route the request to OpenClaw.
"""

import re
import logging

log = logging.getLogger(__name__)

INTENT_PATTERNS = [
    (re.compile(r"(搜一下|搜索|查一下|查询|搜一搜)"), "web_search"),
    (re.compile(r"(发到微信|转发微信|发给微信|微信发送)"), "send_to_wechat"),
    (re.compile(r"(提醒我|分钟后|小时后|点提醒|定时提醒|闹钟)"), "set_reminder"),
    (re.compile(r"(整理文件|归类文件|文件整理)"), "organize_files"),
]


def detect_intent(text: str) -> tuple[str | None, str]:
    """
    Returns (intent_name, original_text) if an OpenClaw-relevant intent is detected,
    or (None, text) if the conversation should just pass through.
    """
    for pattern, intent in INTENT_PATTERNS:
        if pattern.search(text):
            log.info("Intent detected: %s in '%s'", intent, text[:60])
            return intent, text
    return None, text
