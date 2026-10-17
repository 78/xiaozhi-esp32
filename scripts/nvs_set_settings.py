#!/usr/bin/env python3
"""Read-merge-write NVS settings on a XiaoZhi device (or a local image).

The firmware keeps user-facing configuration in NVS, split into namespaces
managed by the Settings class (main/settings.cc). Instead of overwriting the
whole partition (which would wipe unrelated settings), this script:

  1. reads the current NVS partition from flash (esptool read_flash) or a
     local dump,
  2. dumps every live entry (nvs_tool minimal dump),
  3. merges the requested changes (--set / --unset / --ssid family),
  4. regenerates a clean NVS image from the merged entries,
  5. either writes it back over the device, or saves it as a new image file.

Usage
-----
    # set the XiaoZhi server URL and protocol version on the device
    source $IDF_PATH/export.sh
    python3 scripts/nvs_set_settings.py --board lc-s3-wifi-1.54tft \
        --set websocket.url=wss://xiaozhi.me/api --set websocket.version=2

    # provision WiFi (shorthand for --set wifi.ssid / wifi.password)
    python3 scripts/nvs_set_settings.py --board lc-s3-wifi-1.54tft \
        --set wifi.ssid=MyWiFi --set wifi.password=secret --set wifi.channel=11

    # remove a stored token, show the current state
    python3 scripts/nvs_set_settings.py --board lc-s3-wifi-1.54tft --unset websocket.token
    python3 scripts/nvs_set_settings.py --board lc-s3-wifi-1.54tft --list

    # edit a local dump; write the result to a new image file
    python3 scripts/nvs_set_settings.py --board lc-s3-wifi-1.54tft \
        --image nvs_dump.bin --set websocket.url=wss://host/api --output nvs_new.bin

    # list the boards this script knows about
    python3 scripts/nvs_set_settings.py --list-boards

--image and --port are mutually exclusive: the tool either edits a local
image file, or edits a device. --output defaults to <image>.new.bin.
--board is required: chip, and the NVS partition offset/size, always come
from the board's definition (its config.json partition table).

Known settings
--------------
    wifi      ssid, password (string), channel, remember_bssid (u8)
    websocket url, token (string), version (i32)
    mqtt      endpoint, client_id, username, password, publish_topic (string),
              keepalive (i32)
    audio     output_volume (i32)
    display   brightness (i32)
    board     various board-specific keys

For unknown keys the type is inferred from the value: "true"/"false" -> u8,
an integer -> i32, otherwise string.
"""

import argparse
import csv
import glob
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections import OrderedDict

try:
    from serial.tools import list_ports

    HAVE_SERIAL = True
except ImportError:
    HAVE_SERIAL = False

# nvs_tool entry type -> CSV encoding accepted by nvs_partition_gen
ENCODING_MAP = {
    "uint8_t": "u8",
    "int8_t": "i8",
    "uint16_t": "u16",
    "int16_t": "i16",
    "uint32_t": "u32",
    "int32_t": "i32",
    "uint64_t": "u64",
    "int64_t": "i64",
    "string": "string",
    "blob": "base64",
    "blob_data": "base64",
}

# Key types as written by the firmware (Settings / SsidManager). New values
# for these keys must use the same native NVS type or the firmware's typed
# read (nvs_get_str/i32/u8) will fail.
KNOWN_TYPES = {
    ("wifi", "ssid"): "string",
    ("wifi", "password"): "string",
    ("wifi", "channel"): "u8",
    ("wifi", "remember_bssid"): "u8",
    ("websocket", "url"): "string",
    ("websocket", "token"): "string",
    ("websocket", "version"): "i32",
    ("mqtt", "endpoint"): "string",
    ("mqtt", "client_id"): "string",
    ("mqtt", "username"): "string",
    ("mqtt", "password"): "string",
    ("mqtt", "publish_topic"): "string",
    ("mqtt", "keepalive"): "i32",
    ("audio", "output_volume"): "i32",
    ("display", "brightness"): "i32",
}

SECRET_KEYS = ("password", "token")

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOARDS_DIR = os.path.join(PROJECT_ROOT, "main", "boards")

# ESP-IDF target -> chip name accepted by esptool
CHIP_MAP = {"esp32s31": "esp32s3"}

PARTITION_TABLE_RE = re.compile(r'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="([^"]+)"')


def default_partition_csv(target):
    """Mirror sdkconfig.defaults* resolution for a target."""
    override = os.path.join(PROJECT_ROOT, "sdkconfig.defaults.%s" % target)
    for path in (override, os.path.join(PROJECT_ROOT, "sdkconfig.defaults")):
        if not os.path.exists(path):
            continue
        for line in open(path):
            m = PARTITION_TABLE_RE.search(line)
            if m:
                return m.group(1)
    return "partitions/v2/16m.csv"


def find_board_config(name):
    """Return (path, data) for a board given its directory name or a path."""
    candidates = []
    if os.path.exists(name):
        candidates.append(name if name.endswith("config.json") else os.path.join(name, "config.json"))
    candidates.append(os.path.join(BOARDS_DIR, name, "config.json"))
    if not os.path.exists(candidates[-1]) and not os.path.exists(candidates[0]):
        matches = sorted(
            os.path.dirname(p)
            for p in glob.glob(os.path.join(BOARDS_DIR, "**", "config.json"), recursive=True)
            if os.path.basename(os.path.dirname(p)) == name
        )
        if len(matches) == 1:
            candidates.append(os.path.join(matches[0], "config.json"))
        elif len(matches) > 1:
            err("board %r is ambiguous, matches: %s" % (name, ", ".join(matches)))
            sys.exit(1)
    for path in candidates:
        if os.path.exists(path):
            return path, json.load(open(path))
    err("board %r not found under %s (try 'python3 scripts/build.py --list-boards')" % (name, BOARDS_DIR))
    sys.exit(1)


def board_partition_csv(data):
    """Partition table used by a board: custom if configured, else target default."""
    tables = set()
    for build in data.get("builds", []):
        for item in build.get("sdkconfig_append", []):
            m = PARTITION_TABLE_RE.search(item)
            if m:
                tables.add(m.group(1))
    if len(tables) > 1:
        sys.stderr.write(
            "warning: board configures several partition tables (%s); using %r\n"
            % (", ".join(sorted(tables)), sorted(tables)[0])
        )
    if tables:
        return sorted(tables)[0]
    return default_partition_csv(data.get("target", "esp32s3"))


def parse_table_rows(path_or_text):
    """Parse a partition table CSV, accepting a file path or raw text."""
    if os.path.exists(path_or_text):
        text = open(path_or_text).read()
    else:
        text = path_or_text
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        rows.append([c.strip() for c in line.split(",")])
    return rows


def size_to_int(value):
    value = value.strip().upper()
    if value.endswith("K"):
        return int(value[:-1]) * 1024
    if value.endswith("M"):
        return int(value[:-1]) * 1024 * 1024
    return int(value, 0)


def nvs_row(rows):
    """Pick the NVS partition row: by name 'nvs', else 'nvsfactory'."""
    data_rows = [r for r in rows if len(r) >= 5 and r[1] == "data"]
    return (
        next((r for r in data_rows if r[0] == "nvs"), None)
        or next((r for r in data_rows if r[0] == "nvsfactory"), None)
        or None
    )


def resolve_nvs(csv_rel):
    """Resolve (offset, size) of the NVS partition from a partition table CSV.

    Offsets left empty in the table (auto-placed, e.g. v2/32m.csv) are computed
    by round-tripping the table through gen_esp32part.py.
    """
    csv_path = os.path.join(PROJECT_ROOT, csv_rel)
    if not os.path.exists(csv_path):
        err("partition table not found: %s" % csv_path)
        sys.exit(1)
    rows = parse_table_rows(csv_path)
    row = nvs_row(rows)
    if row is None:
        err("no nvs partition in partition table %s" % csv_path)
        sys.exit(1)
    off_str, size_str = row[3], row[4]
    if off_str == "":
        gen = find_tool("gen_esp32part.py", "components/partition_table/gen_esp32part.py")
        if not gen:
            err("gen_esp32part.py not found; source ESP-IDF export.sh first")
            sys.exit(1)
        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
            out_bin = tmp.name
        try:
            run([sys.executable, gen, csv_path, out_bin])
            proc = subprocess.run([sys.executable, gen, out_bin], capture_output=True, text=True)
            if proc.returncode != 0:
                err("could not resolve partition offsets")
                sys.exit(1)
            resolved = nvs_row(parse_table_rows(proc.stdout))
        finally:
            os.unlink(out_bin)
        if resolved is None:
            err("could not resolve NVS offset in %s" % csv_path)
            sys.exit(1)
        off_str, size_str = resolved[3], resolved[4]
    return int(off_str, 0), size_to_int(size_str)


def err(msg):
    sys.stderr.write("error: %s\n" % msg)


def find_tool(name, relpath):
    """Return an absolute path to an IDF tool, preferring IDF_PATH."""
    if os.environ.get("IDF_PATH"):
        candidate = os.path.join(os.environ["IDF_PATH"], relpath)
        if os.path.exists(candidate):
            return candidate
    return shutil.which(name)


def run(cmd, **kw):
    proc = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if proc.returncode != 0:
        err("%s failed (exit %d)" % (" ".join(cmd), proc.returncode))
        sys.stderr.write(proc.stdout)
        sys.stderr.write(proc.stderr)
        sys.exit(1)
    return proc


def esptool_cmd(chip, port):
    """Return an esptool invocation prefix."""
    tool = shutil.which("esptool")
    if tool:
        prefix = [tool]
    else:
        try:
            import esptool  # noqa: F401

            prefix = [sys.executable, "-m", "esptool"]
        except ImportError:
            err("esptool not found; source ESP-IDF export.sh first")
            sys.exit(1)
    cmd = prefix + ["--chip", chip]
    if port:
        cmd += ["-p", port]
    return cmd


def detect_port():
    """Pick a serial port when --port is omitted."""
    if HAVE_SERIAL:
        ports = [p for p in list_ports.comports() if p.device]
        candidates = [
            p for p in ports
            if p.vid is not None
            or any(k in (p.description or "").lower()
                   for k in ("usb", "cp210", "ch34", "jtag", "serial", "uart"))
        ] or ports
        if len(candidates) == 1:
            return candidates[0].device
        if candidates:
            err("multiple serial ports found (%s); pass --port"
                % ", ".join(p.device for p in candidates))
            sys.exit(1)
        err("no serial port detected; pass --port")
        sys.exit(1)
    # Fallback without pyserial: common POSIX device globs
    found = sorted(
        set(
            dev
            for pat in (
                "/dev/cu.usbserial*",
                "/dev/cu.SLAB_USBtoUART*",
                "/dev/cu.usbmodem*",
                "/dev/cu.wchusbserial*",
                "/dev/ttyUSB*",
                "/dev/ttyACM*",
            )
            for dev in glob.glob(pat)
        )
    )
    if len(found) == 1:
        return found[0]
    if found:
        err("multiple serial ports found (%s); pass --port" % ", ".join(found))
        sys.exit(1)
    err("no serial port detected; pass --port (pyserial not available)")
    sys.exit(1)


def read_partition(chip, port, offset, size, out_path):
    run(esptool_cmd(chip, port) + ["read_flash", hex(offset), hex(size), out_path])


def dump_minimal(image_path):
    """Return list of {namespace, key, encoding, data} for live entries."""
    nvs_tool = find_tool(
        "nvs_tool.py", "components/nvs_flash/nvs_partition_tool/nvs_tool.py"
    )
    if not nvs_tool:
        err("nvs_tool.py not found; source ESP-IDF export.sh first")
        sys.exit(1)
    proc = subprocess.run(
        [sys.executable, nvs_tool, "-f", "json", "-d", "minimal", image_path],
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        # All-0xFF (uninitialized) or unreadable partitions yield no entries.
        return []
    try:
        data = json.loads(proc.stdout)
    except json.JSONDecodeError:
        err("could not parse NVS dump from %s" % image_path)
        sys.exit(1)
    return data


def build_entries(dump):
    """Map live dump entries to ordered namespace -> key -> (encoding, value)."""
    entries = OrderedDict()
    skipped = []
    for item in dump:
        if item.get("state") != "Written" or item.get("is_empty"):
            continue
        ns = item.get("namespace")
        key = item.get("key")
        encoding = ENCODING_MAP.get(item.get("encoding"))
        if ns is None or key is None or encoding is None:
            skipped.append(item)
            continue
        value = item.get("data")
        if isinstance(value, bool) or value is None:
            skipped.append(item)
            continue
        entries.setdefault(ns, OrderedDict())[key] = (encoding, str(value))
    if skipped:
        sys.stderr.write(
            "note: skipped %d entry(-ies) with unsupported types\n" % len(skipped)
        )
    return entries


def infer_encoding(ns, key, value):
    """Pick the NVS encoding for a new value: known key type, else inference."""
    known = KNOWN_TYPES.get((ns, key))
    if known is not None:
        if known != "string":
            int(value)  # validate
        return known
    low = value.lower()
    if low in ("true", "false"):
        return "u8"
    if re.fullmatch(r"-?\d+", value):
        return "i32"
    return "string"


def normalize_value(encoding, value):
    """Convert a raw string value to its CSV representation for the encoding."""
    if encoding == "string" or encoding == "base64":
        return value
    if isinstance(value, str):
        low = value.lower()
        if low == "true":
            return "1"
        if low == "false":
            return "0"
    return str(int(value))


def entries_to_csv(entries):
    buf = io.StringIO()
    writer = csv.writer(buf, lineterminator="\n")
    writer.writerow(["key", "type", "encoding", "value"])
    for ns, keys in entries.items():
        writer.writerow([ns, "namespace", "", ""])
        for key, (encoding, value) in keys.items():
            writer.writerow([key, "data", encoding, value])
    return buf.getvalue()


def generate_image(csv_text, size, out_path):
    gen = find_tool(
        "nvs_partition_gen.py",
        "components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py",
    )
    if not gen:
        err("nvs_partition_gen.py not found; source ESP-IDF export.sh first")
        sys.exit(1)
    with tempfile.NamedTemporaryFile(mode="w", suffix=".csv", delete=False) as tmp:
        tmp.write(csv_text)
        tmp_path = tmp.name
    try:
        run([sys.executable, gen, "generate", tmp_path, out_path, hex(size)])
    finally:
        os.unlink(tmp_path)


def verify_image(image_path):
    nvs_tool = find_tool(
        "nvs_tool.py", "components/nvs_flash/nvs_partition_tool/nvs_tool.py"
    )
    if not nvs_tool:
        return
    proc = subprocess.run(
        [sys.executable, nvs_tool, "-i", "-d", "none", image_path],
        capture_output=True,
        text=True,
    )
    status = "ok" if proc.returncode == 0 else "FAILED integrity check"
    sys.stderr.write("new image integrity check: %s\n" % status)


def mask(value):
    return "*" * len(value) if value else value


def display_value(key, encoding, value):
    key = key.lower()
    if encoding == "string" and any(s in key for s in SECRET_KEYS):
        return mask(value)
    return value


def parse_set(ap, text):
    if "=" not in text:
        ap.error("--set expects NS.KEY=VALUE, got %r" % text)
    ns_key, value = text.split("=", 1)
    if "." not in ns_key:
        ap.error("--set key must be NS.KEY, got %r" % ns_key)
    ns, key = ns_key.rsplit(".", 1)
    if not ns or not key:
        ap.error("--set key must be NS.KEY, got %r" % ns_key)
    for bad in ("\n", "\x00"):
        if bad in value:
            ap.error("--set value must not contain newlines or NUL bytes")
    encoding = infer_encoding(ns, key, value)
    return ns, key, encoding, normalize_value(encoding, value)


def parse_unset(ap, text):
    if "." not in text:
        ap.error("--unset expects NS.KEY, got %r" % text)
    ns, key = text.rsplit(".", 1)
    if not ns or not key:
        ap.error("--unset expects NS.KEY, got %r" % text)
    return ns, key


def list_entries(entries):
    for ns, keys in entries.items():
        for key, (encoding, value) in keys.items():
            print("%-12s %-16s %-6s %s" % (
                ns, key, encoding, display_value(key, encoding, value)))


def print_changes(entries, changes, unsets):
    """Print diffs: (ns, key, new_encoding, new_value) sets and (ns, key) unsets."""
    if not changes and not unsets:
        return
    print("changes:")
    for ns, key, encoding, value in changes:
        print("  %s.%s: %s" % (ns, key, display_value(key, encoding, value)))
    for ns, key in unsets:
        print("  %s.%s: <deleted>" % (ns, key))


def list_boards():
    """Relative 'board' values (usable with --board), one per line."""
    boards = []
    for config in glob.glob(os.path.join(BOARDS_DIR, "**", "config.json"), recursive=True):
        boards.append(os.path.relpath(os.path.dirname(config), BOARDS_DIR))
    boards.sort()
    return boards


def main():
    ap = argparse.ArgumentParser(
        description="Read-merge-write NVS settings on a XiaoZhi device or a local image"
    )
    ap.add_argument(
        "--port", help="serial port (default: auto-detected)"
    )
    ap.add_argument(
        "--image",
        help="edit a local NVS dump instead of the device (mutually exclusive with --port)",
    )
    ap.add_argument(
        "--output",
        help="image mode: output file (default: <image>.new.bin)",
    )
    ap.add_argument(
        "--board",
        help="board directory under main/boards; resolves chip and NVS offset/size "
             "(required unless --list-boards)",
    )
    ap.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="NS.KEY=VALUE",
        help="set a setting (repeatable; e.g. websocket.url=...). "
             "Known keys use their firmware type; others are inferred "
             "(true/false -> u8, integer -> i32, else string)",
    )
    ap.add_argument(
        "--unset",
        action="append",
        default=[],
        metavar="NS.KEY",
        help="remove a setting (repeatable)",
    )
    ap.add_argument("--list", action="store_true", help="show all current NVS settings")
    ap.add_argument(
        "--list-boards",
        action="store_true",
        help="list available --board values and exit",
    )
    ap.add_argument("--dry-run", action="store_true", help="don't write anything")
    ap.add_argument("--yes", action="store_true", help="skip the confirmation prompt")
    args = ap.parse_args()

    if args.list_boards:
        print("\n".join(list_boards()))
        return

    if not args.board:
        ap.error("--board is required (see --list-boards)")

    if args.image and args.port:
        ap.error("--image and --port are mutually exclusive: edit a local image, or flash a device")

    changes = []
    for text in args.set:
        changes.append(parse_set(ap, text))
    unsets = [parse_unset(ap, text) for text in args.unset]

    if not changes and not unsets and not args.list:
        ap.error(
            "nothing to do: pass --set/--unset, or --list"
        )

    port = args.port
    if args.image is None and port is None:
        port = detect_port()
        sys.stderr.write("using serial port: %s\n" % port)

    tmpdir = tempfile.mkdtemp(prefix="nvs_settings_")
    dump_path = os.path.join(tmpdir, "nvs_current.bin")
    new_path = os.path.join(tmpdir, "nvs_new.bin")

    cfg_path, data = find_board_config(args.board)
    target = data.get("target", "esp32s3")
    chip = CHIP_MAP.get(target, target)
    table_csv = board_partition_csv(data)
    offset, size = resolve_nvs(table_csv)
    sys.stderr.write(
        "board %s (%s): partition table %s, nvs @ 0x%x (%d bytes)\n"
        % (os.path.basename(os.path.dirname(cfg_path)), chip, table_csv, offset, size)
    )
    if size % 4096 != 0:
        err("NVS size must be a multiple of 4096, got %d" % size)
        sys.exit(1)

    if args.image:
        shutil.copyfile(args.image, dump_path)
        sys.stderr.write("using local NVS image: %s\n" % args.image)
        out_path = args.output or args.image + ".new.bin"
    else:
        read_partition(chip, port, offset, size, dump_path)
        sys.stderr.write("read %d bytes of NVS from 0x%x\n" % (size, offset))
        out_path = new_path

    dump = dump_minimal(dump_path)
    entries = build_entries(dump)

    if args.list:
        print("current NVS settings:")
        list_entries(entries)
        return

    print_changes(entries, changes, unsets)

    for ns, key, encoding, value in changes:
        entries.setdefault(ns, OrderedDict())[key] = (encoding, value)
    for ns, key in unsets:
        entries.get(ns, OrderedDict()).pop(key, None)

    csv_text = entries_to_csv(entries)

    changed_ns = {c[0] for c in changes} | {u[0] for u in unsets}
    preserved_ns = [ns for ns in entries if ns not in changed_ns]
    print("preserving %d other namespace(s): %s" % (
        len(preserved_ns), ", ".join(preserved_ns) or "none"))

    if args.dry_run:
        print("dry-run: nothing written. Merged entries:")
        print(csv_text)
        return

    generate_image(csv_text, size, out_path)
    if not os.path.exists(out_path) or os.path.getsize(out_path) != size:
        err("generated image has unexpected size")
        sys.exit(1)
    verify_image(out_path)

    if args.image:
        print(
            "merged NVS image written to %s (%d bytes); flash it with your usual "
            "tool at offset 0x%x" % (out_path, size, offset)
        )
        return

    if not args.yes:
        answer = input("Write new NVS image to device? [y/N] ").strip().lower()
        if answer not in ("y", "yes"):
            print("aborted")
            return

    run(esptool_cmd(chip, port) + ["write_flash", hex(offset), out_path])


if __name__ == "__main__":
    main()