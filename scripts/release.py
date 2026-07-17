import sys
import os
import json
import zipfile
import argparse
import re
import subprocess
from pathlib import Path
from typing import Optional

# Switch to project root directory
os.chdir(Path(__file__).resolve().parent.parent)

################################################################################
# Common utility functions
################################################################################

def get_board_type_from_compile_commands() -> Optional[str]:
    """Parse the current compiled BOARD_TYPE from build/compile_commands.json"""
    compile_file = Path("build/compile_commands.json")
    if not compile_file.exists():
        return None
    with compile_file.open(encoding='utf-8') as f:
        data = json.load(f)
    for item in data:
        if not item["file"].endswith("main.cc"):
            continue
        cmd = item["command"]
        if "-DBOARD_TYPE=\\\"" in cmd:
            return cmd.split("-DBOARD_TYPE=\\\"")[1].split("\\\"")[0].strip()
    return None


def get_project_version() -> Optional[str]:
    """Read set(PROJECT_VER "x.y.z") from root CMakeLists.txt"""
    with Path("CMakeLists.txt").open(encoding='utf-8') as f:
        for line in f:
            if line.startswith("set(PROJECT_VER"):
                return line.split("\"")[1]
    return None


def merge_bin() -> None:
    if os.system("idf.py merge-bin") != 0:
        print("merge-bin failed", file=sys.stderr)
        sys.exit(1)


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

################################################################################
# board / variant related functions
################################################################################

_BOARDS_DIR = Path("main/boards")


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
            "ESP-IDF version was not detected. Source export.sh before running release.py."
        ) from error


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
        expression = build.get("idf_version")
        if expression and not _version_matches(idf_version, expression):
            continue
        builds.append(dict(build))
    return builds


def _collect_variants(
    config_filename: str = "config.json",
    idf_version: tuple[int, int, int] = (6, 0, 0),
) -> list[dict[str, str]]:
    """Traverse all boards under main/boards, collect variant information.

    Return example:
        [{"board": "bread-compact-ml307", "name": "bread-compact-ml307", "full_name": "bread-compact-ml307"}, ...]
        [{"board": "waveshare/esp32-p4-nano", "name": "esp32-p4-nano-10.1-a", "full_name": "waveshare-esp32-p4-nano-10.1-a"}, ...]
    """
    variants: list[dict[str, str]] = []
    errors: list[str] = []

    for cfg_path in sorted(_BOARDS_DIR.rglob(config_filename)):
        board_dir = cfg_path.parent
        if board_dir.name == "common":
            continue
        board = board_dir.relative_to(_BOARDS_DIR).as_posix()

        try:
            with cfg_path.open(encoding='utf-8') as f:
                cfg = json.load(f)

            manufacturer = _get_manufacturer(cfg)

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

            for build in _get_builds_for_idf(cfg, idf_version):
                name = build["name"]
                full_name = f"{manufacturer}-{name}" if manufacturer else name
                variants.append({
                    "board": board, 
                    "name": name,
                    "full_name": full_name
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
        "scripts/gen_lang.py",
        "scripts/release.py",
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
    board_leaf = board_type.split("/")[-1]
    pattern = f'set(BOARD_TYPE "{board_leaf}")'

    cmake_file = Path("main/CMakeLists.txt")
    lines = cmake_file.read_text(encoding="utf-8").splitlines()
    candidates: list[str] = []

    for idx, line in enumerate(lines):
        if pattern in line:
            # Found the BOARD_TYPE line, search backwards for the nearest config guard
            for back_idx in range(idx - 1, -1, -1):
                back_line = lines[back_idx]
                if "if(CONFIG_BOARD_TYPE_" in back_line:
                    candidates.append(back_line.strip().split("if(")[1].split(")")[0])
                    break
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


def _symbol_supports_target(symbol: str, target: str) -> bool:
    """Check whether Kconfig symbol depends on given target (e.g. esp32c5)."""
    kconfig_file = Path("main/Kconfig.projbuild")
    if not kconfig_file.exists():
        return False

    target_flag = f"IDF_TARGET_{target.upper()}"
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


def _resolve_board_config(board_type: str, target: str, sdkconfig_append: list[str]) -> str:
    """Resolve CONFIG_BOARD_TYPE_xxx for current board build."""
    explicit = _extract_board_config_from_sdkconfig_append(sdkconfig_append)
    if explicit:
        return explicit

    candidates = _find_board_config_candidates(board_type)
    if not candidates:
        raise ValueError(f"Cannot find board config symbol for {board_type}")
    if len(candidates) == 1:
        return candidates[0]

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
    items: list[str] = []
    existing_keys: set[str] = set()

    def _append_if_missing(entry: str) -> None:
        key = entry.split("=", 1)[0]
        if key not in existing_keys:
            items.append(entry)
            existing_keys.add(key)

    # Preserve original order while tracking keys
    for entry in sdkconfig_append:
        _append_if_missing(entry)

    # Apply auto-select rules
    for key, deps in _AUTO_SELECT_RULES.items():
        for entry in sdkconfig_append:
            name, _, value = entry.partition("=")
            if name == key and value.lower().startswith("y"):
                for dep in deps:
                    _append_if_missing(dep)
                break

    return items

################################################################################
# Check board_type in CMakeLists
################################################################################

def _board_type_exists(board_type: str) -> bool:
    cmake_file = Path("main/CMakeLists.txt").read_text(encoding="utf-8")
    board_leaf = board_type.split("/")[-1]
    pattern = f'set(BOARD_TYPE "{board_leaf}")'
    return pattern in cmake_file

################################################################################
# Compile implementation
################################################################################

def release(
    board_type: str,
    config_filename: str = "config.json",
    *,
    filter_name: Optional[str] = None,
    idf_version: tuple[int, int, int] = (6, 0, 0),
) -> None:
    """Compile and package all/specified variants of the specified board_type

    Args:
        board_type: directory name under main/boards
        config_filename: config.json name (default: config.json)
        filter_name: if specified, only compile the build["name"] that matches
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
    manufacturer = _get_manufacturer(cfg)

    builds = _get_builds_for_idf(cfg, idf_version)
    if filter_name:
        builds = [b for b in builds if b["name"] == filter_name]
        if not builds:
            print(f"[ERROR] Variant {filter_name} not found in {board_type}'s {config_filename}", file=sys.stderr)
            sys.exit(1)

    for build in builds:
        name = build["name"]
        board_leaf = board_type.split("/")[-1]

        if board_leaf not in name:
            raise ValueError(f"build.name {name} must contain {board_leaf}")
        
        final_name = f"{manufacturer}-{name}" if manufacturer else name
        output_path = Path("releases") / f"v{project_version}_{final_name}.zip"
        if output_path.exists():
            print(f"Skipping {final_name} because {output_path} already exists")
            continue

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
            board_type_config = _resolve_board_config(board_type, target, build_sdkconfig_append)
            sdkconfig_append = [f"{board_type_config}=y"]
            sdkconfig_append.extend(build_sdkconfig_append)
        sdkconfig_append = _apply_auto_selects(sdkconfig_append)

        print("-" * 80)
        print(f"name: {final_name}")
        print(f"target: {target}")
        if manufacturer:
            print(f"manufacturer: {manufacturer}")
        for item in sdkconfig_append:
            print(f"sdkconfig_append: {item}")

        os.environ.pop("IDF_TARGET", None)

        # Call set-target
        if os.system(f"idf.py set-target {target}") != 0:
            print("set-target failed", file=sys.stderr)
            sys.exit(1)

        # Append sdkconfig
        with Path("sdkconfig").open("a", encoding='utf-8') as f:
            f.write("\n")
            f.write("# Append by release.py\n")
            for append in sdkconfig_append:
                f.write(f"{append}\n")
        # Build with macro BOARD_NAME defined to name
        if os.system(f"idf.py -DBOARD_NAME={name} -DBOARD_TYPE={board_type} build") != 0:
            print("build failed")
            sys.exit(1)

        # merge-bin
        merge_bin()

        # Zip
        zip_bin(final_name, project_version)

################################################################################
# CLI entry
################################################################################

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("board", nargs="?", default=None, help="Board type or 'all'")
    parser.add_argument("-c", "--config", default="config.json", help="Config filename (default: config.json)")
    parser.add_argument("--list-boards", action="store_true", help="List all supported boards and variants")
    parser.add_argument("--json", action="store_true", help="Output in JSON format (use with --list-boards)")
    parser.add_argument("--name", help="Variant name to compile (original name without manufacturer prefix)")
    parser.add_argument(
        "--select-changed",
        action="store_true",
        help="Read changed paths from stdin and output the affected variants as JSON",
    )

    args = parser.parse_args()
    idf_version = _detect_idf_version()

    if args.select_changed:
        variants = _collect_variants(config_filename=args.config, idf_version=idf_version)
        selected = _select_variants_for_changes(variants, sys.stdin.read().splitlines())
        print(json.dumps(selected))
        sys.exit(0)

    # List mode
    if args.list_boards:
        variants = _collect_variants(config_filename=args.config, idf_version=idf_version)
        if args.json:
            print(json.dumps(variants))
        else:
            for v in variants:
                print(f"{v['board']}: {v['name']}")
        sys.exit(0)

    # Current directory firmware packaging mode
    if args.board is None:
        merge_bin()
        curr_board_type = get_board_type_from_compile_commands()
        if curr_board_type is None:
            print("Failed to parse board_type from compile_commands.json", file=sys.stderr)
            sys.exit(1)
        project_ver = get_project_version()
        zip_bin(curr_board_type, project_ver)
        sys.exit(0)

    # Compile mode
    board_type_input: str = args.board
    name_filter: Optional[str] = args.name

    # Check board_type in CMakeLists
    if board_type_input != "all" and not _board_type_exists(board_type_input):
        print(f"[ERROR] board_type {board_type_input} not found in main/CMakeLists.txt", file=sys.stderr)
        sys.exit(1)

    variants_all = _collect_variants(config_filename=args.config, idf_version=idf_version)

    # Filter board_type list
    target_board_types: set[str]
    if board_type_input == "all":
        target_board_types = {v["board"] for v in variants_all}
    else:
        target_board_types = {board_type_input}

    for bt in sorted(target_board_types):
        if not _board_type_exists(bt):
            print(f"[ERROR] board_type {bt} not found in main/CMakeLists.txt", file=sys.stderr)
            sys.exit(1)
        cfg_path = _BOARDS_DIR / bt / args.config
        if bt == board_type_input and not cfg_path.exists():
            print(f"Board {bt} has no {args.config} config file, skipping")
            sys.exit(0)
        release(
            bt,
            config_filename=args.config,
            filter_name=name_filter if bt == board_type_input else None,
            idf_version=idf_version,
        )
