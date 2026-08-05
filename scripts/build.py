#!/usr/bin/env python3

import sys
import os
import json
import zipfile
import argparse
import re
import subprocess
from pathlib import Path
from typing import Any, Optional

# Switch to project root directory
os.chdir(Path(__file__).resolve().parent.parent)

################################################################################
# Common utility functions
################################################################################


_DEFAULT_IDF_VERSION = (6, 0, 2)

_LITE_WAKE_WORD_TARGETS = {"esp32c3", "esp32c5", "esp32c6"}
_AFE_WAKE_WORD_TARGETS = {"esp32s3", "esp32p4", "esp32s31"}
_ESP_WAKE_WORD_TARGETS = {"esp32", *_LITE_WAKE_WORD_TARGETS}
_WAKE_WORD_TARGETS = (
    "esp32",
    "esp32c3",
    "esp32c5",
    "esp32c6",
    "esp32s3",
    "esp32p4",
    "esp32s31",
)
_WAKE_WORD_MODEL_PATTERN = re.compile(r"^wn9[sl]?_[a-z0-9_]+$")
_ESP_SR_KCONFIG = Path(
    "managed_components/espressif__esp-sr/Kconfig.projbuild"
)


def _emit_build_stage(stage: str) -> None:
    """Emit a machine-readable stage marker for cloud build runners."""
    enabled = os.environ.get("XIAOZHI_BUILD_STAGES", "").strip().casefold()
    if enabled in {"1", "true", "yes", "on"}:
        print(f"XIAOZHI_STAGE {stage}", flush=True)


def get_project_version() -> Optional[str]:
    """Read set(PROJECT_VER "x.y.z") from root CMakeLists.txt"""
    with Path("CMakeLists.txt").open(encoding='utf-8') as f:
        for line in f:
            if line.startswith("set(PROJECT_VER"):
                return line.split("\"")[1]
    return None


def _run_idf(*args: str, preview: bool = False) -> None:
    command = ["idf.py"]
    if preview:
        command.append("--preview")
    command.extend(args)
    if subprocess.run(command, check=False).returncode != 0:
        print(f"{' '.join(command)} failed", file=sys.stderr)
        sys.exit(1)


def merge_bin(preview: bool = False) -> None:
    _run_idf("merge-bin", preview=preview)


def zip_bin(name: str, version: str) -> None:
    """Zip build/merged-binary.bin to releases/v{version}_{name}.zip"""
    out_dir = Path("releases")
    out_dir.mkdir(exist_ok=True)
    output_path = out_dir / f"v{version}_{name}.zip"

    if output_path.exists():
        output_path.unlink()

    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as zipf:
        zipf.write("build/merged-binary.bin", arcname="merged-binary.bin")
    print(f"zip bin to {output_path} done")


def _get_manufacturer(cfg: dict) -> Optional[str]:
    """Read manufacturer from config.json"""
    m = cfg.get("manufacturer")
    if isinstance(m, str) and m.strip():
        return m.strip()
    return None


_REPORTED_IDENTIFIER_PATTERN = re.compile(r"^[a-z0-9.-]+$")


def _validate_reported_identifier(value: object, field: str) -> str:
    """Validate an OTA-reported board type or name."""
    if not isinstance(value, str) or not value:
        raise ValueError(f"missing non-empty {field}")
    if not _REPORTED_IDENTIFIER_PATTERN.fullmatch(value):
        raise ValueError(
            f"{field} {value!r} must contain only lowercase letters, "
            'digits, "." and "-"'
        )
    return value


def _get_reported_type(cfg: dict) -> str:
    """Read the compatibility-sensitive board type from config.json."""
    return _validate_reported_identifier(cfg.get("type"), 'top-level "type"')


def _get_reported_name(build: dict) -> str:
    """Read the compatibility-sensitive board name from a build entry."""
    return _validate_reported_identifier(build.get("name"), 'build "name"')


def _get_full_name(manufacturer: Optional[str], name: str) -> str:
    """Return the artifact name without duplicating an existing manufacturer prefix."""
    prefix = f"{manufacturer}-" if manufacturer else ""
    return name if not prefix or name.startswith(prefix) else f"{prefix}{name}"


def _normalize_p4x_release_name(name: str) -> str:
    """Represent P4X as a chip-family segment instead of a trailing suffix."""
    if name.endswith("-p4x"):
        name_without_suffix = name[:-4]
        if "-p4-" in name_without_suffix:
            return name_without_suffix.replace("-p4-", "-p4x-", 1)
    return name


def _get_release_full_name(
    manufacturer: Optional[str],
    build: dict,
) -> str:
    """Return manufacturer + board name for the release artifact."""
    release_name = _get_reported_name(build)
    release_name = _normalize_p4x_release_name(release_name)
    return _get_full_name(manufacturer, release_name)


def _normalize_language(language: str) -> str:
    """Normalize a locale accepted by the build interface."""
    supported_languages = _collect_languages()
    languages_by_casefold = {
        supported.casefold(): supported
        for supported in supported_languages
    }
    normalized = language.strip().replace("_", "-").casefold()
    try:
        return languages_by_casefold[normalized]
    except KeyError as error:
        supported = ", ".join(supported_languages)
        raise ValueError(
            f"Unsupported language {language!r}. Supported values: {supported}"
        ) from error


def _collect_languages(
    cmake_path: Path = Path("main/CMakeLists.txt"),
    kconfig_path: Path = Path("main/Kconfig.projbuild"),
    locales_dir: Path = Path("main/assets/locales"),
) -> list[str]:
    """Read configured locale mappings and validate their build resources."""
    for path in (cmake_path, kconfig_path, locales_dir):
        if not path.exists():
            raise RuntimeError(f"Language configuration source not found: {path}")

    cmake = cmake_path.read_text(encoding="utf-8")
    mappings = re.findall(
        r"(?:if|elseif)\(CONFIG_LANGUAGE_([A-Z_]+)\)\s*"
        r'set\(LANG_DIR "([^"]+)"\)',
        cmake,
    )
    if not mappings:
        raise RuntimeError(f"No language mappings found in {cmake_path}")

    kconfig = kconfig_path.read_text(encoding="utf-8")
    errors: list[str] = []
    languages: list[str] = []
    for symbol, language in mappings:
        if not re.search(
            rf"^\s*config LANGUAGE_{re.escape(symbol)}$",
            kconfig,
            re.MULTILINE,
        ):
            errors.append(f"CONFIG_LANGUAGE_{symbol} is missing from {kconfig_path}")
        if not (locales_dir / language).is_dir():
            errors.append(f"locale directory is missing: {locales_dir / language}")
        if language in languages:
            errors.append(f"duplicate language mapping: {language}")
        else:
            languages.append(language)

    if errors:
        details = "\n".join(f"  - {error}" for error in errors)
        raise RuntimeError(f"Invalid language configuration:\n{details}")
    return languages


def _language_sdkconfig_option(language: str) -> tuple[str, str]:
    """Return the normalized locale and its Kconfig assignment."""
    normalized = _normalize_language(language)
    symbol = normalized.replace("-", "_").upper()
    return normalized, f"CONFIG_LANGUAGE_{symbol}=y"


def _collect_wake_words(
    kconfig_path: Path = _ESP_SR_KCONFIG,
) -> list[dict[str, object]]:
    """Read available WakeNet models from the resolved ESP-SR component."""
    if not kconfig_path.exists():
        raise RuntimeError(
            f"{kconfig_path} was not found. Resolve managed components with "
            "'idf.py reconfigure' before listing wake words."
        )

    content = kconfig_path.read_text(encoding="utf-8")
    config_matches = list(re.finditer(
        r"^\s*config SR_WN_([A-Z0-9_]+)\s*$",
        content,
        re.MULTILINE,
    ))
    wake_words: list[dict[str, object]] = []
    for index, match in enumerate(config_matches):
        block_end = (
            config_matches[index + 1].start()
            if index + 1 < len(config_matches)
            else len(content)
        )
        block = content[match.end():block_end]
        label_match = re.search(r'^\s*bool "([^"]+)"\s*$', block, re.MULTILINE)
        if not label_match:
            continue

        model = match.group(1).lower()
        label = label_match.group(1)
        suffix = f"({model})"
        phrase = (
            label[:-len(suffix)].strip()
            if label.endswith(suffix)
            else label
        )
        targets = (
            _WAKE_WORD_TARGETS
            if model.startswith("wn9s_")
            else (
                "esp32",
                "esp32s3",
                "esp32p4",
                "esp32s31",
            )
        )
        wake_words.append({
            "model": model,
            "phrase": phrase,
            "targets": list(targets),
        })

    if not wake_words:
        raise RuntimeError(f"No WakeNet models found in {kconfig_path}")
    return wake_words


def _print_wake_word_list(wake_words: list[dict[str, object]]) -> None:
    """Print a human-readable table of available wake-word models."""
    model_width = max(
        len("MODEL"),
        *(len(str(item["model"])) for item in wake_words),
    )
    phrase_width = max(
        len("PHRASE"),
        *(len(str(item["phrase"])) for item in wake_words),
    )
    print(f"{'MODEL':<{model_width}}  {'PHRASE':<{phrase_width}}  TARGETS")
    for item in wake_words:
        targets = ",".join(str(target) for target in item["targets"])
        print(
            f"{str(item['model']):<{model_width}}  "
            f"{str(item['phrase']):<{phrase_width}}  {targets}"
        )
    print("\nSpecial values: nihaoxiaozhi, disabled")


def _enabled_default_wake_word_symbols(target: str) -> list[str]:
    """Return wake-word models enabled by the project and target defaults."""
    symbols: list[str] = []
    for path in (Path("sdkconfig.defaults"), Path(f"sdkconfig.defaults.{target}")):
        if not path.exists():
            continue
        for line in path.read_text(encoding="utf-8").splitlines():
            match = re.fullmatch(r"(CONFIG_SR_WN_[A-Z0-9_]+)=y", line.strip())
            if match and match.group(1) not in symbols:
                symbols.append(match.group(1))
    return symbols


def _wake_word_sdkconfig_options(
    wake_word: str,
    target: str,
) -> tuple[str, list[str], list[str]]:
    """Map a wake-word model to implementation and model Kconfig options."""
    normalized = wake_word.strip().casefold().replace("-", "_")
    if normalized == "nihaoxiaozhi":
        normalized = (
            "wn9s_nihaoxiaozhi"
            if target in _LITE_WAKE_WORD_TARGETS
            else "wn9_nihaoxiaozhi_tts"
        )

    model_symbols = _enabled_default_wake_word_symbols(target)
    options = [f"{symbol}=n" for symbol in model_symbols]
    options.extend([
        "CONFIG_WAKE_WORD_DISABLED=n",
        "CONFIG_USE_ESP_WAKE_WORD=n",
        "CONFIG_USE_AFE_WAKE_WORD=n",
        "CONFIG_USE_CUSTOM_WAKE_WORD=n",
    ])

    if normalized == "disabled":
        options.append("CONFIG_WAKE_WORD_DISABLED=y")
        return normalized, options, ["CONFIG_WAKE_WORD_DISABLED"]

    if target not in _ESP_WAKE_WORD_TARGETS | _AFE_WAKE_WORD_TARGETS:
        raise ValueError(f"Wake-word selection is not supported for target {target}")
    if not _WAKE_WORD_MODEL_PATTERN.fullmatch(normalized):
        raise ValueError(
            f"Invalid wake word {wake_word!r}. Use 'disabled', "
            "'nihaoxiaozhi', or an ESP-SR model name such as "
            "'wn9_jarvis_tts'."
        )
    if target in _LITE_WAKE_WORD_TARGETS and not normalized.startswith("wn9s_"):
        raise ValueError(
            f"Target {target} supports WakeNet9s models only; "
            f"{normalized!r} is not compatible"
        )

    implementation = (
        "CONFIG_USE_AFE_WAKE_WORD"
        if target in _AFE_WAKE_WORD_TARGETS
        else "CONFIG_USE_ESP_WAKE_WORD"
    )
    model_symbol = f"CONFIG_SR_WN_{normalized.upper()}"
    options.extend((f"{implementation}=y", f"{model_symbol}=y"))
    return normalized, options, [implementation, model_symbol]

################################################################################
# board / variant related functions
################################################################################

_BOARDS_DIR = Path("main/boards")

_DISPLAY_STYLE_SYMBOLS = {
    "default": "CONFIG_USE_DEFAULT_MESSAGE_STYLE",
    "wechat": "CONFIG_USE_WECHAT_MESSAGE_STYLE",
    "emote": "CONFIG_USE_EMOTE_MESSAGE_STYLE",
}
_DYNAMIC_CAMERA_MIRROR_BOARD_CONFIGS = {
    # These boards intentionally change orientation at runtime according to the
    # detected sensor or persisted device state. A compile-time override would
    # be misleading because that runtime decision would win afterwards.
    "CONFIG_BOARD_TYPE_DF_S3_AI_CAM",
    "CONFIG_BOARD_TYPE_ESP_SPARKBOT",
    "CONFIG_BOARD_TYPE_M5STACK_ATOM_S3R_CAM_M12_ECHO_BASE",
    "CONFIG_BOARD_TYPE_SEEED_STUDIO_SENSECAP_WATCHER",
}


def _sdkconfig_assignments(options: list[str]) -> dict[str, str]:
    """Return the final value for each CONFIG_* assignment in a fragment."""
    assignments: dict[str, str] = {}
    for option in options:
        key, separator, value = option.strip().partition("=")
        if not separator or not key.startswith("CONFIG_"):
            raise ValueError(f"Invalid sdkconfig assignment: {option!r}")
        assignments[key] = value
    return assignments


def _kconfig_choice(
    name: str,
    kconfig_path: Path = Path("main/Kconfig.projbuild"),
) -> dict[str, Any]:
    """Read a named project choice without trying to reimplement Kconfig."""
    content = kconfig_path.read_text(encoding="utf-8")
    start = re.search(rf"^choice\s+{re.escape(name)}\s*$", content, re.MULTILINE)
    if not start:
        raise ValueError(f"Kconfig choice {name} was not found in {kconfig_path}")
    end = re.search(r"^endchoice\s*$", content[start.end():], re.MULTILINE)
    if not end:
        raise ValueError(f"Kconfig choice {name} has no endchoice")
    block = content[start.end():start.end() + end.start()]
    entries: list[dict[str, str]] = []
    matches = list(re.finditer(r"^\s*config\s+([A-Za-z0-9_]+)\s*$", block, re.MULTILINE))
    for index, match in enumerate(matches):
        entry_end = matches[index + 1].start() if index + 1 < len(matches) else len(block)
        entry = block[match.end():entry_end]
        label = re.search(r'^\s*bool\s+"([^"]+)"', entry, re.MULTILINE)
        if label:
            entries.append({"value": match.group(1), "label": label.group(1)})
    default = re.search(r"^\s*default\s+([A-Za-z0-9_]+)", block, re.MULTILINE)
    return {
        "entries": entries,
        "default": default.group(1) if default else None,
        "board_configs": {
            f"CONFIG_{symbol}"
            for symbol in re.findall(r"\b(BOARD_TYPE_[A-Za-z0-9_]+)\b", block)
        },
        "block": block,
    }


def _kconfig_config_board_dependencies(
    symbol: str,
    kconfig_path: Path = Path("main/Kconfig.projbuild"),
) -> set[str]:
    """Return CONFIG_BOARD_TYPE_* names mentioned by one config entry."""
    content = kconfig_path.read_text(encoding="utf-8")
    start = re.search(rf"^\s*config\s+{re.escape(symbol)}\s*$", content, re.MULTILINE)
    if not start:
        raise ValueError(f"Kconfig config {symbol} was not found in {kconfig_path}")
    remainder = content[start.end():]
    end = re.search(r"^\s*(?:config\s+|endchoice\s*$|endmenu\s*$)", remainder, re.MULTILINE)
    block = remainder[:end.start()] if end else remainder
    return {
        f"CONFIG_{item}"
        for item in re.findall(r"\b(BOARD_TYPE_[A-Za-z0-9_]+)\b", block)
    }


def _board_source_text(board: str) -> str:
    board_dir = _BOARDS_DIR / board
    parts: list[str] = []
    for path in sorted(board_dir.rglob("*")):
        if path.suffix in {".c", ".cc", ".cpp", ".h", ".hpp"}:
            parts.append(path.read_text(encoding="utf-8", errors="replace"))
    return "\n".join(parts)


def _selected_choice_default(
    choice: dict[str, Any],
    assignments: dict[str, str],
) -> str:
    for entry in choice["entries"]:
        if assignments.get(f"CONFIG_{entry['value']}") == "y":
            return str(entry["value"])
    default = choice.get("default")
    if not default:
        raise ValueError("Exposed Kconfig choice has no default")
    return str(default)


def _build_option_definitions(
    board: str,
    target: str,
    board_config: str,
    build: dict[str, Any],
) -> list[dict[str, Any]]:
    """Describe the curated, board-compatible options exposed to callers."""
    sdkconfig_append = build.get("sdkconfig_append", [])
    if not isinstance(sdkconfig_append, list) or not all(isinstance(item, str) for item in sdkconfig_append):
        raise ValueError(f"build {build.get('name')!r} sdkconfig_append must be a string list")
    assignments = _sdkconfig_assignments(sdkconfig_append)
    source = _board_source_text(board)
    definitions: list[dict[str, Any]] = []

    for choice_name in ("DISPLAY_OLED_TYPE", "DISPLAY_LCD_TYPE"):
        choice = _kconfig_choice(choice_name)
        if board_config not in choice["board_configs"]:
            continue
        entries = [entry for entry in choice["entries"] if entry["value"] != "LCD_CUSTOM"]
        definitions.append({
            "key": "display_model",
            "type": "select",
            "default": _selected_choice_default(choice, assignments),
            "choices": entries,
        })
        break

    # Message styles are implemented by the color LCD display path. OLED and
    # no-display boards deliberately do not expose a selector that has no effect.
    if re.search(r"\b[A-Za-z0-9_]*LcdDisplay\b", source):
        style_choice = _kconfig_choice("DISPLAY_STYLE")
        emote_boards = _kconfig_config_board_dependencies("USE_EMOTE_MESSAGE_STYLE")
        style_choices = [
            {"value": "default", "label": "Default"},
            {"value": "wechat", "label": "WeChat"},
        ]
        if board_config in emote_boards:
            style_choices.append({"value": "emote", "label": "Emote animation"})
        style_default = "default"
        selected_style = _selected_choice_default(style_choice, assignments)
        for value, symbol in _DISPLAY_STYLE_SYMBOLS.items():
            if symbol == f"CONFIG_{selected_style}":
                style_default = value
                break
        definitions.extend((
            {
                "key": "display_style",
                "type": "select",
                "default": style_default,
                "choices": style_choices,
            },
            {
                "key": "multiline_chat",
                "type": "boolean",
                "default": assignments.get("CONFIG_USE_MULTILINE_CHAT_MESSAGE") == "y",
            },
        ))

    aec_boards = _kconfig_config_board_dependencies("USE_DEVICE_AEC")
    if board_config in aec_boards:
        definitions.append({
            "key": "aec_mode",
            "type": "select",
            "default": "device" if assignments.get("CONFIG_USE_DEVICE_AEC") == "y" else "off",
            "choices": [
                {"value": "off", "label": "Off"},
                {"value": "device", "label": "Device-side AEC"},
            ],
        })

    # ESP32-P4 obtains networking through a companion chip and cannot enable
    # the local ESP-BluFi stack selected by this project option.
    if target != "esp32p4" and ("wifi_board.h" in source or re.search(r"\bWifiBoard\b", source)):
        definitions.append({
            "key": "wifi_provisioning",
            "type": "select",
            "default": (
                "blufi"
                if assignments.get("CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING") == "y"
                and assignments.get("CONFIG_USE_HOTSPOT_WIFI_PROVISIONING") == "n"
                else "hotspot"
            ),
            "choices": [
                {"value": "hotspot", "label": "Wi-Fi hotspot"},
                {"value": "blufi", "label": "ESP-BluFi"},
            ],
        })

    has_common_camera = "new Esp32Camera" in source or "new EspVideo" in source
    if has_common_camera and board_config not in _DYNAMIC_CAMERA_MIRROR_BOARD_CONFIGS:
        definitions.extend((
            {"key": "camera_hmirror", "type": "boolean", "default": False},
            {"key": "camera_vflip", "type": "boolean", "default": False},
        ))

    configured_defaults = build.get("build_options", {})
    if not isinstance(configured_defaults, dict):
        raise ValueError(f"build {build.get('name')!r} build_options must be an object")
    by_key = {definition["key"]: definition for definition in definitions}
    unknown_defaults = sorted(set(configured_defaults) - set(by_key))
    if unknown_defaults:
        raise ValueError(
            f"build {build.get('name')!r} has unsupported build_options defaults: "
            + ", ".join(unknown_defaults)
        )
    for key, value in configured_defaults.items():
        by_key[key]["default"] = value

    # Validate board defaults through the same path used for caller input.
    _normalize_build_options(definitions, {})
    return definitions


def _normalize_build_options(
    definitions: list[dict[str, Any]],
    requested: object,
) -> dict[str, object]:
    if not isinstance(requested, dict):
        raise ValueError("--build-options-json must contain a JSON object")
    by_key = {definition["key"]: definition for definition in definitions}
    unknown = sorted(set(requested) - set(by_key))
    if unknown:
        raise ValueError("Unsupported build option(s): " + ", ".join(unknown))

    normalized: dict[str, object] = {}
    for key in sorted(by_key):
        definition = by_key[key]
        value = requested.get(key, definition["default"])
        if definition["type"] == "boolean":
            if not isinstance(value, bool):
                raise ValueError(f"Build option {key} must be a boolean")
        elif definition["type"] == "select":
            allowed = {choice["value"] for choice in definition["choices"]}
            if not isinstance(value, str) or value not in allowed:
                raise ValueError(
                    f"Build option {key} must be one of: {', '.join(sorted(allowed))}"
                )
        normalized[key] = value

    if normalized.get("display_style") != "default" and "multiline_chat" in normalized:
        normalized["multiline_chat"] = False
    return normalized


def _build_options_sdkconfig(
    definitions: list[dict[str, Any]],
    options: dict[str, object],
    base_assignments: dict[str, str],
) -> list[str]:
    """Expand semantic build options into a complete, mutually-exclusive fragment."""
    by_key = {definition["key"]: definition for definition in definitions}
    result: list[str] = []

    if "display_model" in options:
        selected = options["display_model"]
        for choice in by_key["display_model"]["choices"]:
            result.append(f"CONFIG_{choice['value']}={'y' if choice['value'] == selected else 'n'}")
        if isinstance(selected, str) and selected.startswith("LCD_"):
            # LCD_CUSTOM is intentionally not exposed in the cloud UI because
            # it requires source-level panel configuration, but it is still a
            # sibling in the Kconfig choice and must be disabled explicitly.
            result.append("CONFIG_LCD_CUSTOM=n")

    if "display_style" in options:
        selected = options["display_style"]
        for value, symbol in _DISPLAY_STYLE_SYMBOLS.items():
            result.append(f"{symbol}={'y' if value == selected else 'n'}")
        flash_symbols = (
            "CONFIG_FLASH_NONE_ASSETS",
            "CONFIG_FLASH_DEFAULT_ASSETS",
            "CONFIG_FLASH_CUSTOM_ASSETS",
            "CONFIG_FLASH_EXPRESSION_ASSETS",
        )
        if selected == "emote" and base_assignments.get("CONFIG_FLASH_CUSTOM_ASSETS") != "y":
            result.extend(
                f"{symbol}={'y' if symbol == 'CONFIG_FLASH_EXPRESSION_ASSETS' else 'n'}"
                for symbol in flash_symbols
            )
        elif selected != "emote" and base_assignments.get("CONFIG_FLASH_EXPRESSION_ASSETS") == "y":
            result.extend(
                f"{symbol}={'y' if symbol == 'CONFIG_FLASH_DEFAULT_ASSETS' else 'n'}"
                for symbol in flash_symbols
            )

    if "multiline_chat" in options:
        result.append(f"CONFIG_USE_MULTILINE_CHAT_MESSAGE={'y' if options['multiline_chat'] else 'n'}")

    if "aec_mode" in options:
        device = options["aec_mode"] == "device"
        result.extend((
            f"CONFIG_USE_DEVICE_AEC={'y' if device else 'n'}",
            "CONFIG_USE_SERVER_AEC=n",
        ))
        if device:
            result.append("CONFIG_USE_AUDIO_PROCESSOR=y")

    if "wifi_provisioning" in options:
        blufi = options["wifi_provisioning"] == "blufi"
        result.extend((
            f"CONFIG_USE_HOTSPOT_WIFI_PROVISIONING={'n' if blufi else 'y'}",
            f"CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING={'y' if blufi else 'n'}",
        ))

    if "camera_hmirror" in options or "camera_vflip" in options:
        result.extend((
            "CONFIG_XIAOZHI_CAMERA_MIRROR_CONFIGURED=y",
            f"CONFIG_XIAOZHI_CAMERA_HMIRROR={'y' if options.get('camera_hmirror') else 'n'}",
            f"CONFIG_XIAOZHI_CAMERA_VFLIP={'y' if options.get('camera_vflip') else 'n'}",
        ))
    return result


def _parse_version(value: str) -> tuple[int, int, int]:
    """Parse an ESP-IDF version string such as 5.5.4 or v6.0."""
    match = re.search(r"v?(\d+)\.(\d+)(?:\.(\d+))?", value)
    if not match:
        raise ValueError(f"Invalid ESP-IDF version: {value}")
    return tuple(int(part or 0) for part in match.groups())


def _detect_idf_version() -> tuple[int, int, int]:
    """Resolve the active ESP-IDF version for version-gated build variants."""
    idf_path = os.environ.get("IDF_PATH")
    if idf_path:
        version_file = Path(idf_path) / "tools/cmake/version.cmake"
        if version_file.exists():
            values: dict[str, int] = {}
            for line in version_file.read_text(encoding="utf-8").splitlines():
                match = re.match(r"set\(IDF_VERSION_(MAJOR|MINOR|PATCH)\s+(\d+)\)", line)
                if match:
                    values[match.group(1)] = int(match.group(2))
            if all(part in values for part in ("MAJOR", "MINOR", "PATCH")):
                return values["MAJOR"], values["MINOR"], values["PATCH"]

    try:
        output = subprocess.run(
            ["idf.py", "--version"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        return _parse_version(output)
    except (FileNotFoundError, subprocess.CalledProcessError, ValueError) as error:
        raise RuntimeError(
            "ESP-IDF version was not detected. Source export.sh before running build.py."
        ) from error


def _detect_idf_version_for_listing() -> tuple[int, int, int]:
    """Detect IDF when available, otherwise list the preferred IDF 6.0 variants."""
    try:
        return _detect_idf_version()
    except RuntimeError:
        version = ".".join(str(part) for part in _DEFAULT_IDF_VERSION)
        print(
            f"[WARN] ESP-IDF is not active; listing variants for ESP-IDF {version}.",
            file=sys.stderr,
        )
        return _DEFAULT_IDF_VERSION


def _version_matches(version: tuple[int, int, int], expression: str) -> bool:
    """Evaluate a single comparison such as '<6.0' or '>=6.0.1'."""
    match = re.fullmatch(r"\s*(<=|>=|<|>|==)\s*(v?\d+\.\d+(?:\.\d+)?)\s*", expression)
    if not match:
        raise ValueError(f"Invalid ESP-IDF version expression: {expression}")
    operator, expected_text = match.groups()
    expected = _parse_version(expected_text)
    return {
        "<": version < expected,
        "<=": version <= expected,
        ">": version > expected,
        ">=": version >= expected,
        "==": version == expected,
    }[operator]


def _get_builds_for_idf(cfg: dict, idf_version: tuple[int, int, int]) -> list[dict]:
    """Return build entries whose optional ESP-IDF version rule matches."""
    builds: list[dict] = []
    for build in cfg.get("builds", []):
        _get_reported_name(build)
        expression = build.get("idf_version")
        if expression and not _version_matches(idf_version, expression):
            continue
        builds.append(dict(build))
    return builds


def _get_board_display_name(
    config_symbol: str,
    kconfig_path: Path = Path("main/Kconfig.projbuild"),
) -> str:
    """Return the user-facing bool prompt for a CONFIG_BOARD_TYPE_* symbol."""
    symbol = config_symbol.removeprefix("CONFIG_")
    if not kconfig_path.exists():
        raise ValueError(f"Board Kconfig file not found: {kconfig_path}")

    in_symbol = False
    for line in kconfig_path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if stripped.startswith("config "):
            if in_symbol:
                break
            in_symbol = stripped.split("config ", 1)[1].strip() == symbol
            continue
        if not in_symbol:
            continue
        prompt = re.match(r'^bool\s+"([^"]+)"', stripped)
        if prompt:
            return prompt.group(1)
        if stripped.startswith(("choice ", "endchoice", "menu ", "endmenu")):
            break

    raise ValueError(
        f'Kconfig bool prompt not found for {config_symbol} in {kconfig_path}'
    )


def _collect_variants(
    config_filename: str = "config.json",
    idf_version: tuple[int, int, int] = (6, 0, 0),
) -> list[dict[str, Any]]:
    """Traverse all boards under main/boards, collect variant information.

    Return example:
        [{"board": "bread-compact-ml307", "name": "bread-compact-ml307", "full_name": "bread-compact-ml307"}, ...]
        [{"board": "waveshare/esp32-p4-nano", "name": "esp32-p4-nano-10.1-a", "full_name": "waveshare-esp32-p4-nano-10.1-a"}, ...]
    """
    variants: list[dict[str, str]] = []
    errors: list[str] = []
    type_owners: dict[str, str] = {}
    name_owners: dict[str, str] = {}
    identity_owners: dict[tuple[str, str], str] = {}

    for cfg_path in sorted(_BOARDS_DIR.rglob(config_filename)):
        board_dir = cfg_path.parent
        if board_dir.name == "common":
            continue
        board = board_dir.relative_to(_BOARDS_DIR).as_posix()

        try:
            with cfg_path.open(encoding='utf-8') as f:
                cfg = json.load(f)

            manufacturer = _get_manufacturer(cfg)
            reported_type = _get_reported_type(cfg)
            target = cfg.get("target")
            if not isinstance(target, str) or not target:
                raise ValueError(f"{cfg_path}: missing non-empty target")

            previous_board = type_owners.get(reported_type)
            if previous_board is not None and previous_board != board:
                errors.append(
                    f"duplicate reported board type {reported_type!r} in "
                    f"{previous_board} and {board}"
                )
            else:
                type_owners[reported_type] = board

            # Check manufacturer consistency with directory structure
            if "/" in board:
                # Board is in a subdirectory (e.g., waveshare/esp32-p4-nano)
                expected_manufacturer = board.split("/")[0]
                if not manufacturer:
                    errors.append(
                        f"{cfg_path}: Board is in '{expected_manufacturer}/' subdirectory, "
                        f"but config.json is missing \"manufacturer\": \"{expected_manufacturer}\""
                    )
                elif manufacturer != expected_manufacturer:
                    errors.append(
                        f"{cfg_path}: manufacturer mismatch, "
                        f"directory is '{expected_manufacturer}/' but config.json has \"{manufacturer}\""
                    )
            else:
                # Board is directly under boards/ directory
                if manufacturer:
                    errors.append(
                        f"{cfg_path}: Board is not in a manufacturer subdirectory, "
                        f"but config.json defines manufacturer \"{manufacturer}\", "
                        f"please move board to main/boards/{manufacturer}/{board}/"
                    )

            builds = _get_builds_for_idf(cfg, idf_version)
            for build in builds:
                name = _get_reported_name(build)
                full_name = _get_release_full_name(manufacturer, build)

                previous_config = name_owners.get(name)
                if previous_config is not None:
                    errors.append(
                        f"duplicate reported board name {name!r} in "
                        f"{previous_config} and {cfg_path}"
                    )
                else:
                    name_owners[name] = str(cfg_path)

                identity = (reported_type, name)
                previous_config = identity_owners.get(identity)
                if previous_config is not None:
                    errors.append(
                        f"duplicate reported board identity {identity!r} in "
                        f"{previous_config} and {cfg_path}"
                    )
                else:
                    identity_owners[identity] = str(cfg_path)

                variants.append({
                    "board": board,
                    "name": name,
                    "full_name": full_name,
                    "type": reported_type,
                    "target": target,
                })

        except Exception as e:
            errors.append(f"{cfg_path}: {e}")

    seen_names: dict[str, str] = {}
    for variant in variants:
        previous_board = seen_names.get(variant["full_name"])
        if previous_board is not None:
            errors.append(
                f"duplicate artifact name {variant['full_name']!r} in "
                f"{previous_board} and {variant['board']}"
            )
        else:
            seen_names[variant["full_name"]] = variant["board"]

    if errors:
        details = "\n".join(f"  - {error}" for error in errors)
        raise ValueError(f"Invalid board configuration:\n{details}")

    # Enrich only after the compatibility identities above are validated, so
    # duplicate/malformed config errors remain the primary actionable failure.
    for variant in variants:
        cfg_path = _BOARDS_DIR / variant["board"] / config_filename
        with cfg_path.open(encoding="utf-8") as file:
            cfg = json.load(file)
        build = next(
            item for item in _get_builds_for_idf(cfg, idf_version)
            if _get_reported_name(item) == variant["name"]
        )
        sdkconfig_append = build.get("sdkconfig_append", [])
        if not isinstance(sdkconfig_append, list) or not all(
            isinstance(item, str) for item in sdkconfig_append
        ):
            raise ValueError(
                f'{cfg_path}: build {variant["name"]!r} '
                'sdkconfig_append must be a string list'
            )
        config_symbol = _resolve_board_config(
            variant["board"],
            variant["target"],
            sdkconfig_append,
            variant_name=variant["name"],
        )
        variant["config"] = config_symbol
        variant["display_name"] = _get_board_display_name(config_symbol)
        variant["build_options"] = _build_option_definitions(
            variant["board"],
            variant["target"],
            config_symbol,
            build,
        )

    return sorted(variants, key=lambda variant: (variant["board"], variant["name"]))


def _select_variants_for_changes(
    variants: list[dict[str, str]], changed_files: list[str]
) -> list[dict[str, str]]:
    """Select variants affected by a git diff.

    Board ownership is resolved using the longest known board directory prefix,
    so nested paths such as waveshare/esp32-c6-touch-amoled-2.06 are preserved.
    """
    known_boards = sorted({variant["board"] for variant in variants}, key=len, reverse=True)
    affected: set[str] = set()
    global_paths = {
        ".github/workflows/build.yml",
        "CMakeLists.txt",
        "scripts/build_default_assets.py",
        "scripts/build.py",
        "scripts/gen_lang.py",
        "scripts/versions.py",
    }

    for raw_path in changed_files:
        path = raw_path.strip()
        if not path:
            continue
        if (path in global_paths or path.startswith("components/") or
                path.startswith("partitions/") or
                path.startswith("sdkconfig.defaults") or
                (path.startswith("main/") and not path.startswith("main/boards/")) or
                path.startswith("main/boards/common/")):
            return variants

        prefix = "main/boards/"
        if path.startswith(prefix):
            relative = path[len(prefix):]
            board = next(
                (candidate for candidate in known_boards
                 if relative == candidate or relative.startswith(f"{candidate}/")),
                None,
            )
            if board is not None:
                affected.add(board)

    return [variant for variant in variants if variant["board"] in affected]



def _find_board_config_candidates(board_type: str) -> list[str]:
    """Find all CONFIG_BOARD_TYPE_xxx candidates for the given board_type."""
    board_path = board_type.strip("/")
    lines = Path("main/CMakeLists.txt").read_text(encoding="utf-8").splitlines()
    candidates: list[str] = []
    branch_symbol: Optional[str] = None
    branch_lines: list[str] = []

    def finish_branch() -> None:
        if branch_symbol is None:
            return

        branch = "\n".join(branch_lines)
        directory_match = re.search(r'set\(BOARD_DIR\s+"([^"]+)"\)', branch)
        directory = directory_match.group(1) if directory_match else None
        if directory == board_path:
            candidates.append(branch_symbol)

    condition_pattern = re.compile(r"^\s*(?:if|elseif)\(([^)]+)\)")
    for line in lines:
        condition_match = condition_pattern.match(line)
        if condition_match:
            finish_branch()
            condition = condition_match.group(1)
            branch_symbol = (
                condition if condition.startswith("CONFIG_BOARD_TYPE_") else None
            )
            branch_lines = []
        elif re.match(r"^\s*(?:else|endif)\b", line):
            finish_branch()
            branch_symbol = None
            branch_lines = []
        elif branch_symbol is not None:
            branch_lines.append(line)
    finish_branch()

    return candidates


def _extract_board_config_from_sdkconfig_append(sdkconfig_append: list[str]) -> Optional[str]:
    """Extract explicit CONFIG_BOARD_TYPE_xxx=y from sdkconfig_append, if present."""
    pattern = re.compile(r"^(CONFIG_BOARD_TYPE_[A-Za-z0-9_]+)=y$")
    matches = []
    for item in sdkconfig_append:
        m = pattern.match(item.strip())
        if m:
            matches.append(m.group(1))
    if not matches:
        return None
    uniq = list(dict.fromkeys(matches))
    if len(uniq) > 1:
        raise ValueError(f"Multiple board type configs found in sdkconfig_append: {uniq}")
    return uniq[0]


def _board_config_symbol_exists(
    symbol: str,
    kconfig_path: Path = Path("main/Kconfig.projbuild"),
) -> bool:
    """Return whether a CONFIG_BOARD_TYPE_* symbol exists in project Kconfig."""
    if not kconfig_path.exists():
        return False
    name = symbol.removeprefix("CONFIG_")
    return bool(re.search(
        rf"^\s*config\s+{re.escape(name)}\s*$",
        kconfig_path.read_text(encoding="utf-8"),
        re.MULTILINE,
    ))


def _symbol_supports_target(symbol: str, target: str) -> bool:
    """Check whether Kconfig symbol depends on given target (e.g. esp32c5)."""
    kconfig_file = Path("main/Kconfig.projbuild")
    if not kconfig_file.exists():
        return False

    target_flag = f"IDF_TARGET_{target.upper()}"
    symbol = symbol.removeprefix("CONFIG_")
    lines = kconfig_file.read_text(encoding="utf-8").splitlines()

    in_symbol = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("config "):
            curr_symbol = stripped.split("config ", 1)[1].strip()
            in_symbol = curr_symbol == symbol
            continue
        if in_symbol and stripped.startswith(("config ", "choice ", "endchoice", "menu ", "endmenu")):
            break
        if in_symbol and "depends on" in stripped and target_flag in stripped:
            return True
    return False


def _resolve_board_config(
    board_type: str,
    target: str,
    sdkconfig_append: list[str],
    *,
    variant_name: Optional[str] = None,
) -> str:
    """Resolve CONFIG_BOARD_TYPE_xxx for current board build."""
    explicit = _extract_board_config_from_sdkconfig_append(sdkconfig_append)
    if explicit and _board_config_symbol_exists(explicit):
        return explicit
    if explicit:
        print(
            f"[WARN] Explicit board config {explicit} does not exist in Kconfig; "
            f"resolving it from BOARD_DIR={board_type!r} instead.",
            file=sys.stderr,
        )

    candidates = _find_board_config_candidates(board_type)
    if not candidates:
        raise ValueError(f"Cannot find board config symbol for {board_type}")
    if len(candidates) == 1:
        return candidates[0]

    if variant_name:
        expected = "CONFIG_BOARD_TYPE_" + re.sub(
            r"[^A-Z0-9]+",
            "_",
            variant_name.upper(),
        ).strip("_")
        by_variant = [candidate for candidate in candidates if candidate == expected]
        if len(by_variant) == 1:
            return by_variant[0]

    by_target = [c for c in candidates if _symbol_supports_target(c, target)]
    if len(by_target) == 1:
        return by_target[0]
    if len(by_target) > 1:
        selected = by_target[0]
        print(
            f"[WARN] Ambiguous board config for {board_type} (target={target}), "
            f"target-matched candidates={by_target}, selecting first: {selected}",
            file=sys.stderr,
        )
        return selected

    target_u = target.upper()
    target_short = target_u.replace("ESP32", "")
    by_name = [
        c for c in candidates
        if target_u in c or f"_{target_short}" in c
    ]
    if len(by_name) == 1:
        return by_name[0]
    if len(by_name) > 1:
        selected = by_name[0]
        print(
            f"[WARN] Ambiguous board config for {board_type} (target={target}), "
            f"name-matched candidates={by_name}, selecting first: {selected}",
            file=sys.stderr,
        )
        return selected

    selected = candidates[0]
    print(
        f"[WARN] Ambiguous board config for {board_type} (target={target}), "
        f"candidates={candidates}, selecting first: {selected}",
        file=sys.stderr,
    )
    return selected


# Kconfig "select" entries are not automatically applied when we simply append
# sdkconfig lines from config.json, so add the required dependencies here to
# mimic menuconfig behaviour.
_AUTO_SELECT_RULES: dict[str, list[str]] = {
    "CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING": [
        "CONFIG_BT_ENABLED=y",
        "CONFIG_BT_BLUEDROID_ENABLED=y",
        "CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y",
        "CONFIG_BT_BLE_50_FEATURES_SUPPORTED=n",
        "CONFIG_BT_BLE_BLUFI_ENABLE=y",
    ],
}


def _apply_auto_selects(sdkconfig_append: list[str]) -> list[str]:
    """Apply hardcoded auto-select rules to sdkconfig_append."""
    items = list(sdkconfig_append)

    # Apply auto-select rules
    for key, deps in _AUTO_SELECT_RULES.items():
        for entry in sdkconfig_append:
            name, _, value = entry.partition("=")
            if name == key and value.lower().startswith("y"):
                # A board preset may explicitly disable a dependency. Kconfig's
                # select would override it, so the generated defaults fragment
                # must do the same instead of keeping the earlier value.
                items = _merge_sdkconfig_options(items, deps)
                break

    return items


def _merge_sdkconfig_options(
    base_options: list[str],
    override_options: list[str],
) -> list[str]:
    """Merge sdkconfig assignments by key, with later overrides winning."""
    keys: list[str] = []
    values: dict[str, str] = {}
    for option in (*base_options, *override_options):
        key = option.split("=", 1)[0]
        if key not in values:
            keys.append(key)
        values[key] = option
    return [values[key] for key in keys]


################################################################################
# Check board_type in CMakeLists
################################################################################

def _board_type_exists(board_type: str) -> bool:
    return bool(_find_board_config_candidates(board_type))

################################################################################
# Compile implementation
################################################################################


def _target_from_sdkconfig() -> Optional[str]:
    sdkconfig = Path("sdkconfig")
    if not sdkconfig.exists():
        return None
    match = re.search(
        r'^CONFIG_IDF_TARGET="([^"]+)"$',
        sdkconfig.read_text(encoding="utf-8"),
        re.MULTILINE,
    )
    return match.group(1) if match else None


def _target_from_cmake_cache() -> Optional[str]:
    cache = Path("build/CMakeCache.txt")
    if not cache.exists():
        return None
    match = re.search(
        r"^IDF_TARGET(?::[^=]+)?=(.+)$",
        cache.read_text(encoding="utf-8"),
        re.MULTILINE,
    )
    return match.group(1).strip() if match else None


def _configured_target() -> Optional[str]:
    """Return the current target when sdkconfig and CMake state agree."""
    sdkconfig_target = _target_from_sdkconfig()
    cache_target = _target_from_cmake_cache()
    if sdkconfig_target and cache_target and sdkconfig_target != cache_target:
        return None
    return cache_target or sdkconfig_target


def _sync_vscode_target(
    target: str,
    settings_path: Path = Path(".vscode/settings.json"),
) -> bool:
    """Keep an existing VS Code ESP-IDF target setting in sync."""
    if not settings_path.exists():
        return False

    content = settings_path.read_text(encoding="utf-8")
    pattern = re.compile(
        r'(?P<prefix>"IDF_TARGET"\s*:\s*")'
        r'(?:\\.|[^"\\])*'
        r'(?P<suffix>")'
    )
    updated, matches = pattern.subn(
        lambda match: f'{match.group("prefix")}{target}{match.group("suffix")}',
        content,
    )
    if matches == 0 or updated == content:
        return False

    settings_path.write_text(updated, encoding="utf-8")
    print(f"[INFO] Updated {settings_path} IDF_TARGET to {target}.")
    return True


def _prepare_target(target: str, preview: bool) -> None:
    """Clean only when an existing CMake build uses another target."""
    cache_target = _target_from_cmake_cache()
    current_target = _configured_target()
    if current_target == target:
        print(f"[INFO] Reusing target {target}.")
        return

    if cache_target and cache_target != target:
        print(f"[INFO] Switching target from {cache_target} to {target}.")
        _run_idf("fullclean", preview=preview)
    elif current_target:
        print(f"[INFO] Configuring target {target} (was {current_target}).")
    else:
        print(f"[INFO] Configuring target {target}.")


def _configure_build(
    target: str,
    sdkconfig_append: list[str],
    board_name: str,
    preview: bool,
) -> None:
    """Configure target, board identity and sdkconfig defaults in one CMake run."""
    sdkconfig = Path("sdkconfig")
    sdkconfig_old = Path("sdkconfig.old")
    if sdkconfig.exists():
        if sdkconfig_old.exists():
            sdkconfig_old.unlink()
        sdkconfig.replace(sdkconfig_old)

    fragment = Path("build/xiaozhi-build.sdkconfig.defaults")
    fragment.parent.mkdir(parents=True, exist_ok=True)
    fragment.write_text(
        "# Generated by scripts/build.py\n"
        + "\n".join(sdkconfig_append)
        + "\n",
        encoding="utf-8",
    )

    defaults = []
    if Path("sdkconfig.defaults").exists():
        defaults.append("sdkconfig.defaults")
    defaults.append(fragment.as_posix())
    _run_idf(
        f"-DIDF_TARGET={target}",
        f"-DSDKCONFIG_DEFAULTS={';'.join(defaults)}",
        f"-DBOARD_NAME={board_name}",
        "reconfigure",
        preview=preview,
    )
    _sync_vscode_target(target)


def _validate_configured_symbols(symbols: list[str], option_name: str) -> None:
    """Ensure Kconfig accepted every user-selected build option."""
    if not symbols:
        return
    sdkconfig = Path("sdkconfig")
    if not sdkconfig.exists():
        raise RuntimeError(
            f"Cannot validate {option_name}: sdkconfig was not generated"
        )
    configured = sdkconfig.read_text(encoding="utf-8")
    missing = [
        symbol
        for symbol in symbols
        if not re.search(rf"^{re.escape(symbol)}=y$", configured, re.MULTILINE)
    ]
    if missing:
        raise ValueError(
            f"{option_name} is incompatible with this board or ESP-IDF "
            f"configuration; Kconfig rejected: {', '.join(missing)}"
        )


def _validate_configured_options(options: list[str], option_name: str) -> None:
    """Ensure Kconfig accepted each requested y/n semantic build option."""
    if not options:
        return
    sdkconfig = Path("sdkconfig")
    if not sdkconfig.exists():
        raise RuntimeError(f"Cannot validate {option_name}: sdkconfig was not generated")
    content = sdkconfig.read_text(encoding="utf-8")
    rejected: list[str] = []
    for option in options:
        key, _, expected = option.partition("=")
        if expected == "y":
            accepted = bool(re.search(rf"^{re.escape(key)}=y$", content, re.MULTILINE))
        elif expected == "n":
            accepted = bool(re.search(
                rf"^(?:# {re.escape(key)} is not set|{re.escape(key)}=n)$",
                content,
                re.MULTILINE,
            ))
        else:
            continue
        if not accepted:
            rejected.append(option)
    if rejected:
        raise ValueError(
            f"{option_name} is incompatible with this board or ESP-IDF "
            f"configuration; Kconfig rejected: {', '.join(rejected)}"
        )


def build_board(
    board_type: str,
    config_filename: str = "config.json",
    *,
    name_filter: str,
    create_zip: bool = False,
    language: Optional[str] = None,
    wake_word: Optional[str] = None,
    build_options: Optional[dict[str, object]] = None,
    idf_version: tuple[int, int, int] = (6, 0, 0),
) -> None:
    """Compile one specified variant of the specified board type.

    Args:
        board_type: directory name under main/boards
        config_filename: config.json name (default: config.json)
        name_filter: build["name"] to compile
        create_zip: package merged-binary.bin under releases/ when true
        language: optional locale such as en-US
        wake_word: optional ESP-SR model name or "disabled"
        build_options: optional semantic, board-validated option values
    """
    cfg_path = _BOARDS_DIR / Path(board_type) / config_filename
    if not cfg_path.exists():
        print(f"[WARN] {cfg_path} does not exist, skipping {board_type}")
        return

    project_version = get_project_version()
    print(f"Project Version: {project_version} ({cfg_path})")

    with cfg_path.open(encoding='utf-8') as f:
        cfg = json.load(f)
    target = cfg["target"]
    reported_type = _get_reported_type(cfg)
    preview = cfg.get("preview", False)
    if not isinstance(preview, bool):
        raise ValueError(f"{cfg_path}: preview must be a boolean")
    manufacturer = _get_manufacturer(cfg)

    builds = _get_builds_for_idf(cfg, idf_version)
    builds = [
        build for build in builds
        if _get_reported_name(build) == name_filter
    ]
    if not builds:
        print(
            f"[ERROR] Variant {name_filter} not found in "
            f"{board_type}'s {config_filename}",
            file=sys.stderr,
        )
        sys.exit(1)

    for build in builds:
        name = _get_reported_name(build)
        final_name = _get_release_full_name(manufacturer, build)

        # Process sdkconfig_append
        build_sdkconfig_append = build.get("sdkconfig_append", [])
        explicit_board_cfg = _extract_board_config_from_sdkconfig_append(build_sdkconfig_append)
        if explicit_board_cfg:
            print(
                f"[INFO] Board config explicitly set in config.json: {explicit_board_cfg}, "
                "skip auto-select.",
            )
            sdkconfig_append = list(build_sdkconfig_append)
        else:
            board_type_config = _resolve_board_config(
                board_type,
                target,
                build_sdkconfig_append,
                variant_name=name,
            )
            sdkconfig_append = [f"{board_type_config}=y"]
            sdkconfig_append.extend(build_sdkconfig_append)

        option_definitions = _build_option_definitions(
            board_type,
            target,
            board_type_config if not explicit_board_cfg else explicit_board_cfg,
            build,
        )

        user_options: list[str] = []
        validation_symbols: list[tuple[list[str], str]] = []
        build_option_sdkconfig: list[str] = []
        selected_language = None
        selected_wake_word = None
        selected_build_options: Optional[dict[str, object]] = None
        if language is not None:
            selected_language, option = _language_sdkconfig_option(language)
            user_options.append(option)
            validation_symbols.append(
                ([option.split("=", 1)[0]], "--language")
            )
        if wake_word is not None:
            (
                selected_wake_word,
                wake_word_options,
                wake_word_symbols,
            ) = _wake_word_sdkconfig_options(wake_word, target)
            user_options.extend(wake_word_options)
            validation_symbols.append((wake_word_symbols, "--wake-word"))

        # A build entry may carry semantic defaults to preserve an existing
        # board-specific behavior after it moves out of hardcoded C++.
        if build_options is not None or "build_options" in build:
            selected_build_options = _normalize_build_options(
                option_definitions,
                build_options or {},
            )
            build_option_sdkconfig = _build_options_sdkconfig(
                option_definitions,
                selected_build_options,
                _sdkconfig_assignments(build_sdkconfig_append),
            )
            user_options.extend(build_option_sdkconfig)

        sdkconfig_append = _merge_sdkconfig_options(
            sdkconfig_append,
            user_options,
        )
        sdkconfig_append = _apply_auto_selects(sdkconfig_append)

        print("-" * 80)
        print(f"name: {final_name}")
        if final_name != name:
            print(f"reported_name: {name}")
        print(f"reported_type: {reported_type}")
        print(f"target: {target}")
        if manufacturer:
            print(f"manufacturer: {manufacturer}")
        if selected_language:
            print(f"language: {selected_language}")
        if selected_wake_word:
            print(f"wake_word: {selected_wake_word}")
        if selected_build_options is not None:
            print(
                "build_options: "
                + json.dumps(selected_build_options, ensure_ascii=False, sort_keys=True)
            )
        for item in sdkconfig_append:
            print(f"sdkconfig_append: {item}")

        _emit_build_stage("dependencies_resolving")
        os.environ.pop("IDF_TARGET", None)
        _prepare_target(target, preview)
        _configure_build(
            target,
            sdkconfig_append,
            name,
            preview,
        )
        for symbols, option_name in validation_symbols:
            _validate_configured_symbols(symbols, option_name)
        _validate_configured_options(build_option_sdkconfig, "--build-options-json")

        # build.name is the compatibility-sensitive OTA-reported board identity.
        _emit_build_stage("compiling")
        _run_idf("build", preview=preview)

        # merge-bin
        _emit_build_stage("packaging")
        merge_bin(preview)

        if create_zip:
            zip_bin(final_name, project_version)

################################################################################
# CLI entry
################################################################################


def _print_board_list(variants: list[dict[str, str]]) -> None:
    by_board: dict[str, list[str]] = {}
    for variant in variants:
        by_board.setdefault(variant["board"], []).append(variant["name"])

    for board, names in by_board.items():
        print(board)
        if len(names) > 1:
            for name in names:
                print(f"  - {name}")


def _select_variant(board: str, variants: list[dict[str, str]]) -> str:
    board_variants = [variant for variant in variants if variant["board"] == board]
    if not board_variants:
        print(f"[ERROR] No build variants found for {board}.", file=sys.stderr)
        sys.exit(1)
    if len(board_variants) == 1:
        return board_variants[0]["name"]

    print(f"Available variants for {board}:")
    for index, variant in enumerate(board_variants, start=1):
        print(f"  {index}. {variant['name']}")

    if not sys.stdin.isatty():
        print(
            "[ERROR] Multiple variants found in non-interactive mode; "
            "select one with --name.",
            file=sys.stderr,
        )
        sys.exit(2)

    while True:
        try:
            answer = input(f"Select a variant [1-{len(board_variants)}]: ").strip()
        except EOFError:
            print("\n[ERROR] No variant selected.", file=sys.stderr)
            sys.exit(2)
        if answer.isdigit() and 1 <= int(answer) <= len(board_variants):
            return board_variants[int(answer) - 1]["name"]
        for variant in board_variants:
            if answer == variant["name"]:
                return variant["name"]
        print("Enter a listed number or variant name.")


def main(argv: Optional[list[str]] = None) -> None:
    parser = argparse.ArgumentParser(
        description="Configure and build one XiaoZhi board variant.",
    )
    parser.add_argument("board", nargs="?", default=None, help="Board type or 'all'")
    parser.add_argument("-c", "--config", default="config.json", help="Config filename (default: config.json)")
    parser.add_argument("--list-boards", action="store_true", help="List all supported boards and variants")
    parser.add_argument(
        "--list-languages",
        action="store_true",
        help="List values accepted by --language",
    )
    parser.add_argument(
        "--list-wake-words",
        action="store_true",
        help="List wake-word models provided by the current ESP-SR component",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output list results in JSON format",
    )
    parser.add_argument("--name", help="build.name to compile (the OTA-reported board name)")
    parser.add_argument(
        "--language",
        metavar="LOCALE",
        help="Firmware language locale, for example zh-CN or en-US",
    )
    parser.add_argument(
        "--wake-word",
        metavar="MODEL",
        help=(
            "Wake-word model (for example wn9_jarvis_tts), "
            "'nihaoxiaozhi', or 'disabled'"
        ),
    )
    parser.add_argument(
        "--build-options-json",
        metavar="JSON",
        help=(
            "Semantic board options as a JSON object. Accepted keys are "
            "reported by --list-boards --json for the selected variant."
        ),
    )
    parser.add_argument(
        "--zip",
        action="store_true",
        help="Also recreate releases/v<version>_<name>.zip",
    )
    parser.add_argument(
        "--select-changed",
        action="store_true",
        help="Read changed paths from stdin and output the affected variants as JSON",
    )

    cli_args = sys.argv[1:] if argv is None else argv
    if not cli_args:
        parser.print_help()
        return
    args = parser.parse_args(cli_args)

    if args.select_changed:
        if (
            args.board
            or args.list_boards
            or args.list_languages
            or args.list_wake_words
            or args.name
            or args.language
            or args.wake_word
            or args.build_options_json
            or args.zip
            or args.json
        ):
            parser.error("--select-changed cannot be combined with build or list options")
        idf_version = _detect_idf_version_for_listing()
        variants = _collect_variants(config_filename=args.config, idf_version=idf_version)
        selected = _select_variants_for_changes(variants, sys.stdin.read().splitlines())
        print(json.dumps(selected))
        return

    if args.list_languages:
        if (
            args.board is not None
            or args.list_boards
            or args.list_wake_words
            or args.name
            or args.language
            or args.wake_word
            or args.build_options_json
            or args.zip
        ):
            parser.error(
                "--list-languages cannot be combined with build or other "
                "list options"
            )
        languages = _collect_languages()
        if args.json:
            print(json.dumps(languages))
        else:
            print("\n".join(languages))
        return

    if args.list_wake_words:
        if (
            args.board is not None
            or args.list_boards
            or args.name
            or args.language
            or args.wake_word
            or args.build_options_json
            or args.zip
        ):
            parser.error(
                "--list-wake-words cannot be combined with build or other "
                "list options"
            )
        try:
            wake_words = _collect_wake_words()
        except RuntimeError as error:
            print(f"[ERROR] {error}", file=sys.stderr)
            sys.exit(1)
        if args.json:
            print(json.dumps(wake_words, ensure_ascii=False))
        else:
            _print_wake_word_list(wake_words)
        return

    if args.list_boards:
        if args.board is not None:
            parser.error("--list-boards does not accept a board")
        if (
            args.list_languages
            or args.list_wake_words
            or args.zip
            or args.name
            or args.language
            or args.wake_word
            or args.build_options_json
        ):
            parser.error(
                "--list-boards cannot be combined with build or other "
                "list options"
            )
        idf_version = _detect_idf_version_for_listing()
        variants = _collect_variants(config_filename=args.config, idf_version=idf_version)
        if args.json:
            print(json.dumps(variants))
        else:
            _print_board_list(variants)
        return

    if args.board is None:
        parser.error("a board is required unless --list-boards is used")

    # Compile mode
    board_type_input: str = args.board
    name_filter: Optional[str] = args.name
    idf_version = _detect_idf_version()
    if args.json:
        parser.error("--json is only valid when listing boards")
    if board_type_input == "all" and name_filter:
        parser.error("--name cannot be combined with board 'all'")
    if board_type_input == "all" and args.build_options_json:
        parser.error("--build-options-json cannot be combined with board 'all'")

    parsed_build_options: Optional[dict[str, object]] = None
    if args.build_options_json is not None:
        try:
            raw_build_options = json.loads(args.build_options_json)
        except json.JSONDecodeError as error:
            parser.error(f"--build-options-json is invalid JSON: {error}")
        if not isinstance(raw_build_options, dict):
            parser.error("--build-options-json must contain a JSON object")
        parsed_build_options = raw_build_options

    # Check board_type in CMakeLists
    if board_type_input != "all" and not _board_type_exists(board_type_input):
        print(f"[ERROR] board_type {board_type_input} not found in main/CMakeLists.txt", file=sys.stderr)
        sys.exit(1)

    variants_all = _collect_variants(config_filename=args.config, idf_version=idf_version)

    if board_type_input == "all":
        selected_variants = variants_all
    else:
        if name_filter is None:
            name_filter = _select_variant(board_type_input, variants_all)
        selected_variants = [
            variant
            for variant in variants_all
            if variant["board"] == board_type_input
            and variant["name"] == name_filter
        ]
        if not selected_variants:
            print(
                f"[ERROR] Variant {name_filter} not found for {board_type_input}.",
                file=sys.stderr,
            )
            sys.exit(1)

    for variant in selected_variants:
        bt = variant["board"]
        if not _board_type_exists(bt):
            print(f"[ERROR] board_type {bt} not found in main/CMakeLists.txt", file=sys.stderr)
            sys.exit(1)
        cfg_path = _BOARDS_DIR / bt / args.config
        if bt == board_type_input and not cfg_path.exists():
            print(f"Board {bt} has no {args.config} config file, skipping")
            return
        build_board(
            bt,
            config_filename=args.config,
            name_filter=variant["name"],
            create_zip=args.zip,
            language=args.language,
            wake_word=args.wake_word,
            build_options=parsed_build_options,
            idf_version=idf_version,
        )


if __name__ == "__main__":
    main()
