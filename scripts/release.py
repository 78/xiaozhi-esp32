import sys
import os
import json
import zipfile
import argparse
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
    with compile_file.open() as f:
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
    with Path("CMakeLists.txt").open() as f:
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

################################################################################
# board / variant related functions
################################################################################

_BOARDS_DIR = Path("main/boards")


def _collect_variants(config_filename: str = "config.json") -> list[dict[str, str]]:
    """Traverse all boards under main/boards, collect variant information.

    Return example:
        [{"board": "bread-compact-ml307", "name": "bread-compact-ml307"}, ...]
    """
    variants: list[dict[str, str]] = []
    for board_path in _BOARDS_DIR.iterdir():
        if not board_path.is_dir():
            continue
        if board_path.name == "common":
            continue
        cfg_path = board_path / config_filename
        if not cfg_path.exists():
            print(f"[WARN] {cfg_path} does not exist, skip", file=sys.stderr)
            continue
        try:
            with cfg_path.open() as f:
                cfg = json.load(f)
            for build in cfg.get("builds", []):
                variants.append({"board": board_path.name, "name": build["name"]})
        except Exception as e:
            print(f"[ERROR] 解析 {cfg_path} 失败: {e}", file=sys.stderr)
    return variants


def _parse_board_config_map() -> dict[str, str]:
    """Build the mapping of CONFIG_BOARD_TYPE_xxx and board_type from main/CMakeLists.txt"""
    cmake_file = Path("main/CMakeLists.txt")
    mapping: dict[str, str] = {}
    lines = cmake_file.read_text(encoding="utf-8").splitlines()
    for idx, line in enumerate(lines):
        if "if(CONFIG_BOARD_TYPE_" in line:
            config_name = line.strip().split("if(")[1].split(")")[0]
            if idx + 1 < len(lines):
                next_line = lines[idx + 1].strip()
                if next_line.startswith("set(BOARD_TYPE"):
                    board_type = next_line.split('"')[1]
                    mapping[config_name] = board_type
    return mapping


def _find_board_config(board_type: str) -> Optional[str]:
    """Find the corresponding CONFIG_BOARD_TYPE_xxx for the given board_type"""
    for config, b_type in _parse_board_config_map().items():
        if b_type == board_type:
            return config
    return None


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
        "CONFIG_MBEDTLS_DHM_C=y",
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
    cmake_file = Path("main/CMakeLists.txt")
    pattern = f'set(BOARD_TYPE "{board_type}")'
    return pattern in cmake_file.read_text(encoding="utf-8")

################################################################################
# Compile implementation
################################################################################

def release(board_type: str, config_filename: str = "config.json", *, filter_name: Optional[str] = None) -> None:
    """Compile and package all/specified variants of the specified board_type

    Args:
        board_type: directory name under main/boards
        config_filename: config.json name (default: config.json)
        filter_name: if specified, only compile the build["name"] that matches
    """
    cfg_path = _BOARDS_DIR / board_type / config_filename
    if not cfg_path.exists():
        print(f"[WARN] {cfg_path} 不存在，跳过 {board_type}")
        return

    project_version = get_project_version()
    print(f"Project Version: {project_version} ({cfg_path})")

    with cfg_path.open() as f:
        cfg = json.load(f)
    target = cfg["target"]

    builds = cfg.get("builds", [])
    if filter_name:
        builds = [b for b in builds if b["name"] == filter_name]
        if not builds:
            print(f"[ERROR] 未在 {board_type} 的 {config_filename} 中找到变体 {filter_name}", file=sys.stderr)
            sys.exit(1)

    for build in builds:
        name = build["name"]
        if not name.startswith(board_type):
            raise ValueError(f"build.name {name} 必须以 {board_type} 开头")

        output_path = Path("releases") / f"v{project_version}_{name}.zip"
        if output_path.exists():
            print(f"跳过 {name} 因为 {output_path} 已存在")
            continue

        # Process sdkconfig_append
        board_type_config = _find_board_config(board_type)
        sdkconfig_append = [f"{board_type_config}=y"]
        sdkconfig_append.extend(build.get("sdkconfig_append", []))
        sdkconfig_append = _apply_auto_selects(sdkconfig_append)

        print("-" * 80)
        print(f"name: {name}")
        print(f"target: {target}")
        for item in sdkconfig_append:
            print(f"sdkconfig_append: {item}")

        os.environ.pop("IDF_TARGET", None)

        # Call set-target
        if os.system(f"idf.py set-target {target}") != 0:
            print("set-target failed", file=sys.stderr)
            sys.exit(1)

        # Append sdkconfig
        with Path("sdkconfig").open("a") as f:
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
        zip_bin(name, project_version)

################################################################################
# CLI entry
################################################################################

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("board", nargs="?", default=None, help="板子类型或 all")
    parser.add_argument("-c", "--config", default="config.json", help="指定 config 文件名，默认 config.json")
    parser.add_argument("--list-boards", action="store_true", help="列出所有支持的 board 及变体列表")
    parser.add_argument("--json", action="store_true", help="配合 --list-boards，JSON 格式输出")
    parser.add_argument("--name", help="指定变体名称，仅编译匹配的变体")

    args = parser.parse_args()

    # List mode
    if args.list_boards:
        variants = _collect_variants(config_filename=args.config)
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
            print("未能从 compile_commands.json 解析 board_type", file=sys.stderr)
            sys.exit(1)
        project_ver = get_project_version()
        zip_bin(curr_board_type, project_ver)
        sys.exit(0)

    # Compile mode
    board_type_input: str = args.board
    name_filter: str | None = args.name

    # Check board_type in CMakeLists
    if board_type_input != "all" and not _board_type_exists(board_type_input):
        print(f"[ERROR] main/CMakeLists.txt 中未找到 board_type {board_type_input}", file=sys.stderr)
        sys.exit(1)

    variants_all = _collect_variants(config_filename=args.config)

    # Filter board_type list
    target_board_types: set[str]
    if board_type_input == "all":
        target_board_types = {v["board"] for v in variants_all}
    else:
        target_board_types = {board_type_input}

    for bt in sorted(target_board_types):
        if not _board_type_exists(bt):
            print(f"[ERROR] main/CMakeLists.txt 中未找到 board_type {bt}", file=sys.stderr)
            sys.exit(1)
        cfg_path = _BOARDS_DIR / bt / args.config
        if bt == board_type_input and not cfg_path.exists():
            print(f"开发板 {bt} 未定义 {args.config} 配置文件，跳过")
            sys.exit(0)
        release(bt, config_filename=args.config, filter_name=name_filter if bt == board_type_input else None)
