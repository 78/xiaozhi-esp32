#!/usr/bin/env python3
"""Build the Lila firmware from a .env file.

`scripts/build.py` is the canonical build entry point, but its curated options
cannot express API credentials, a MultiNet custom wake word, or the board's
LCD/OLED choice. This wrapper reads a `.env` file (see `.env.example`) and turns
it into `scripts/build.py` arguments plus raw `--sdkconfig` overrides, so a Lila
build needs no hand editing of sources or sdkconfig.

Everything is optional: with an empty `.env` it builds a Spanish `wk-esp32s3-dev`
firmware named Lila that wakes on the custom word "lila".
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

DEFAULTS = {
    "LILA_BOARD": "wk-esp32s3-dev",
    "LILA_NAME": "wk-esp32s3-dev",
    "LILA_LANGUAGE": "es-ES",
    "LILA_WAKE_WORD": "custom:lila",
    # The reference build uses a 7-pin ST7789 240x240 (SPI mode 3). Use
    # LCD_ST7789_240X240 instead for modules wired with a dedicated CS line.
    "LILA_DISPLAY": "LCD_ST7789_240X240_7PIN",
    "LILA_TAVILY_API_KEY": "",
    "LILA_TELEGRAM_BOT_TOKEN": "",
    "LILA_TELEGRAM_CHAT_ID": "",
    "LILA_SDKCONFIG": "",
    "LILA_ZIP": "0",
}


def load_env_file(path: Path) -> dict[str, str]:
    """Parse simple KEY=VALUE lines, ignoring blanks, comments and quoting."""
    values: dict[str, str] = {}
    if not path.exists():
        return values
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        key = key.strip()
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        values[key] = value
    return values


def resolve_setting(env_file: dict[str, str], key: str) -> str:
    """Process environment wins over the .env file, which wins over defaults."""
    if key in os.environ:
        return os.environ[key]
    return env_file.get(key, DEFAULTS[key])


def require_clean(value: str, key: str) -> str:
    if any(character in value for character in ('"', "\n", "\r")):
        raise SystemExit(f"{key} must not contain quotes or newlines")
    return value


def display_assignments(display: str) -> list[str]:
    """Map a panel symbol to the board's display choice plus panel selection."""
    display = display.strip()
    if not display:
        return []
    symbol = display.upper()
    if symbol.startswith("LCD_"):
        return [
            "CONFIG_WK_ESP32S3_DEV_DISPLAY_LCD=y",
            "CONFIG_WK_ESP32S3_DEV_DISPLAY_OLED=n",
            f"CONFIG_{symbol}=y",
        ]
    if symbol.startswith("OLED_"):
        return [
            "CONFIG_WK_ESP32S3_DEV_DISPLAY_OLED=y",
            "CONFIG_WK_ESP32S3_DEV_DISPLAY_LCD=n",
            f"CONFIG_{symbol}=y",
        ]
    raise SystemExit(
        f"LILA_DISPLAY={display!r} is invalid; use an LCD_* or OLED_* symbol, "
        "for example LCD_ST7789_240X240_7PIN"
    )


def build_command(settings: dict[str, str]) -> list[str]:
    command = [
        sys.executable,
        str(REPO_ROOT / "scripts" / "build.py"),
        settings["LILA_BOARD"],
        "--name",
        settings["LILA_NAME"],
        "--language",
        settings["LILA_LANGUAGE"],
        "--wake-word",
        settings["LILA_WAKE_WORD"],
    ]

    sdkconfig: list[str] = []
    sdkconfig += display_assignments(settings["LILA_DISPLAY"])

    tavily = settings["LILA_TAVILY_API_KEY"]
    if tavily:
        sdkconfig.append(
            f'CONFIG_XIAOZHI_TAVILY_API_KEY="{require_clean(tavily, "LILA_TAVILY_API_KEY")}"'
        )
    token = settings["LILA_TELEGRAM_BOT_TOKEN"]
    if token:
        sdkconfig.append(
            f'CONFIG_XIAOZHI_TELEGRAM_BOT_TOKEN='
            f'"{require_clean(token, "LILA_TELEGRAM_BOT_TOKEN")}"'
        )
    chat_id = settings["LILA_TELEGRAM_CHAT_ID"]
    if chat_id:
        sdkconfig.append(
            f'CONFIG_XIAOZHI_TELEGRAM_CHAT_ID="{require_clean(chat_id, "LILA_TELEGRAM_CHAT_ID")}"'
        )

    # Raw escape hatch: LILA_SDKCONFIG="CONFIG_FOO=y CONFIG_BAR=3"
    for extra in shlex.split(settings["LILA_SDKCONFIG"]):
        sdkconfig.append(extra)

    for assignment in sdkconfig:
        command += ["--sdkconfig", assignment]

    if settings["LILA_ZIP"].strip().lower() in {"1", "true", "yes", "on"}:
        command.append("--zip")

    return command


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--env-file",
        default=str(REPO_ROOT / ".env"),
        help="Path to the .env file (default: <repo>/.env)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the build command without running it",
    )
    args = parser.parse_args()

    env_file = load_env_file(Path(args.env_file))
    settings = {key: resolve_setting(env_file, key) for key in DEFAULTS}
    command = build_command(settings)

    rendered = " ".join(shlex.quote(part) for part in command)
    if args.dry_run:
        print(rendered)
        return 0

    print("Running:", rendered)
    return subprocess.call(command, cwd=REPO_ROOT)


if __name__ == "__main__":
    sys.exit(main())
