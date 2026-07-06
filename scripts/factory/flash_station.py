#!/usr/bin/env python3
"""Tuni factory flashing station (robo-worker spec 2026-07-06 factory-provisioning §4.6).

Per-device operator flow (one command, ~2 min):
  1. Preflight: esptool chip check — chip type + revision must match the image
     variant (tuni-p4 vs tuni-p4-p4x mix-up guard).
  2. Flash the merged factory image at 0x0, -b 230400 (the known-reliable rate
     for the P4-NANO USB-JTAG port), hard reset. Never opens a serial reader
     while esptool holds the port.
  3. Read serial (115200, DTR/RTS deasserted) until the FACTORY|... boot line
     reports the C6 WiFi MAC (= fleet Device-Id). Only running firmware can
     read that MAC — esptool on the P4 port cannot.
  4. POST /api/factory/devices with FACTORY_STATION_TOKEN; server returns the
     one-time claim code.
  5. Append factory-records.csv (full sha256 of the code, NEVER the plaintext)
     and print the label text + fragment claim URL (+ QR PNG when the optional
     `qrcode` package is installed). Lost label => re-run: the code rotates.

Test hooks: --self-test (offline unit checks), --mock-serial FILE + --dry-run
(full flow, no hardware, no server writes).

Usage:
  python scripts/factory/flash_station.py --port /dev/cu.usbmodemXXX \
      --image releases/v1.8.9_tuni-p4.zip --api-url https://robo-worker.example \
      --hw-rev p4-nano-v1.3 --batch B001
  FACTORY_STATION_TOKEN comes from the environment.
"""

import argparse
import csv
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

MAC_RE = re.compile(r"^[0-9a-f]{2}(:[0-9a-f]{2}){5}$")
FACTORY_PREFIX = "FACTORY|"
SERIAL_BAUD = 115200
FLASH_BAUD = "230400"  # gotcha: 460800 drops the P4-NANO USB-JTAG port
FACTORY_LINE_TIMEOUT_S = 90


# --- FACTORY line ----------------------------------------------------------

def parse_factory_line(line):
    """Parse `FACTORY|device_id=..|fw=..` -> dict, or None if not a factory line.

    Unknown keys are ignored by contract (Phase B appends pubkey_jwk=<json>).
    device_id is normalized to lowercase and validated as a colon MAC.
    """
    line = line.strip()
    idx = line.find(FACTORY_PREFIX)
    if idx < 0:
        return None
    fields = {}
    # pubkey_jwk is JSON and may contain no '|' (base64url + punctuation), so a
    # plain split on '|' is safe for all specced keys.
    for part in line[idx + len(FACTORY_PREFIX):].split("|"):
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        # Strip ANSI escapes / control chars that log formatters may append —
        # a polluted fw value would otherwise land in the CSV.
        value = re.sub(r"\x1b\[[0-9;]*m", "", value)
        value = "".join(ch for ch in value if ch.isprintable())
        fields[key.strip()] = value.strip()
    device_id = fields.get("device_id", "").lower()
    if not MAC_RE.match(device_id):
        return None
    fields["device_id"] = device_id
    return fields


# --- Label / records -------------------------------------------------------

def render_label(device_id, claim_code, claim_base_url):
    claim_url = f"{claim_base_url}#d={device_id}&c={claim_code}"  # fragment: never hits server logs
    label = f"TUNI {device_id}  CODE {claim_code}"
    return label, claim_url


def append_record(records_path, device_id, hw_rev, batch, fw_version, claim_code):
    """CSV row keyed by the FULL sha256 of the code — plaintext is never persisted."""
    code_sha256 = hashlib.sha256(claim_code.encode()).hexdigest()
    path = Path(records_path)
    is_new = not path.exists()
    with path.open("a", newline="") as f:
        writer = csv.writer(f)
        if is_new:
            writer.writerow(["ts", "device_id", "hw_rev", "batch", "fw_version", "code_sha256"])
        writer.writerow([
            datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
            device_id,
            hw_rev or "",
            batch or "",
            fw_version or "",
            code_sha256,
        ])
    return code_sha256


def write_qr_png(claim_url, out_path):
    try:
        import qrcode  # optional dependency
    except ImportError:
        return False
    qrcode.make(claim_url).save(out_path)
    return True


# --- Flash + identity capture ----------------------------------------------

def resolve_image(image_arg):
    """Accept a merged .bin or a release .zip containing merged-binary.bin."""
    path = Path(image_arg)
    if not path.exists():
        sys.exit(f"image not found: {path}")
    if path.suffix == ".bin":
        return path
    if path.suffix == ".zip":
        tmp = Path(tempfile.mkdtemp(prefix="tuni-factory-"))
        with zipfile.ZipFile(path) as zf:
            name = "merged-binary.bin"
            if name not in zf.namelist():
                sys.exit(f"{path} does not contain {name} (not a release.py zip?)")
            zf.extract(name, tmp)
        return tmp / name
    sys.exit(f"unsupported image type: {path} (want .bin or .zip)")


def run_esptool(args_list):
    cmd = [sys.executable, "-m", "esptool", *args_list]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit(f"esptool failed: {' '.join(cmd)}\n{proc.stdout}\n{proc.stderr}")
    return proc.stdout


def preflight_chip(port, expected_chip, expected_rev_prefix):
    """Read chip id; refuse to flash the wrong image variant (spec §4.6.1)."""
    out = run_esptool(["--port", port, "--baud", FLASH_BAUD, "chip_id"])
    match = re.search(r"Chip is (\S+) \(revision (v[\d.]+)\)", out)
    if not match:
        sys.exit(f"could not parse chip id from esptool output:\n{out}")
    chip, rev = match.group(1).lower(), match.group(2)
    if expected_chip.lower() not in chip:
        sys.exit(f"chip mismatch: found {chip}, image expects {expected_chip}")
    if expected_rev_prefix and not rev.startswith(expected_rev_prefix):
        sys.exit(
            f"chip revision {rev} does not match expected {expected_rev_prefix}* — "
            f"wrong image variant? (tuni-p4 vs tuni-p4-p4x)"
        )
    print(f"[station] chip ok: {chip} {rev}")


def flash_image(port, image_path):
    print(f"[station] flashing {image_path.name} @ {FLASH_BAUD} ...")
    run_esptool([
        "--chip", "auto", "--port", port, "--baud", FLASH_BAUD,
        "--before", "default_reset", "--after", "hard_reset",
        "write_flash", "0x0", str(image_path),
    ])
    print("[station] flash done, device resetting")


def wait_for_factory_line(port, timeout_s=FACTORY_LINE_TIMEOUT_S):
    """Open serial AFTER esptool released the port; DTR/RTS deasserted so we
    don't reset the board or wedge USB-JTAG (repo gotcha). The USB-Serial-JTAG
    node re-enumerates after esptool's hard reset, so opening is retried
    within the deadline instead of dying on the first SerialException."""
    import serial  # pyserial; present in the ESP-IDF environment

    deadline = datetime.datetime.now() + datetime.timedelta(seconds=timeout_s)
    ser = None
    try:
        while datetime.datetime.now() < deadline and ser is None:
            try:
                ser = serial.Serial()
                ser.port = port
                ser.baudrate = SERIAL_BAUD
                ser.timeout = 2
                ser.dtr = False
                ser.rts = False
                ser.open()
            except serial.SerialException:
                ser = None
                import time

                time.sleep(1)  # port re-enumerating after hard reset
        if ser is None:
            sys.exit(f"could not open {port} within {timeout_s}s — replug the USB cable (USB-JTAG re-enumeration)")
        while datetime.datetime.now() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            parsed = parse_factory_line(raw.decode("utf-8", errors="replace"))
            if parsed:
                return parsed
    finally:
        if ser is not None and ser.is_open:
            ser.close()
    sys.exit(
        f"no FACTORY line within {timeout_s}s — the line prints on network-connect "
        f"AND in wifi-config mode, so check power/serial wiring"
    )


def factory_lines_from_file(path):
    for line in Path(path).read_text().splitlines():
        parsed = parse_factory_line(line)
        if parsed:
            return parsed
    sys.exit(f"no FACTORY line found in {path}")


# --- Registration -----------------------------------------------------------

def register_device(api_url, token, identity, hw_rev, batch):
    body = {"deviceId": identity["device_id"]}
    if hw_rev:
        body["hwRev"] = hw_rev
    if batch:
        body["batch"] = batch
    if "pubkey_jwk" in identity:  # Phase B firmware
        body["publicKey"] = json.loads(identity["pubkey_jwk"])
    req = urllib.request.Request(
        f"{api_url.rstrip('/')}/api/factory/devices",
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json", "Authorization": f"Bearer {token}"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as err:
        detail = err.read().decode("utf-8", errors="replace")
        if err.code == 409:
            sys.exit(f"[station] device is already claimed — run the RMA unclaim first.\n{detail}")
        sys.exit(f"[station] register failed HTTP {err.code}: {detail}")


# --- Self-test ---------------------------------------------------------------

def self_test():
    line = "I (1234) FACTORY|device_id=9C:13:9E:D7:7F:B8|fw=1.8.9"
    parsed = parse_factory_line(line)
    assert parsed == {"device_id": "9c:13:9e:d7:7f:b8", "fw": "1.8.9"}, parsed

    phase_b = 'FACTORY|device_id=aa:bb:cc:dd:ee:ff|fw=2.0.0|pubkey_jwk={"kty":"EC","x":"abc"}|future_key=1'
    parsed_b = parse_factory_line(phase_b)
    assert parsed_b is not None and json.loads(parsed_b["pubkey_jwk"])["kty"] == "EC", parsed_b
    assert parsed_b["future_key"] == "1"  # unknown keys pass through, ignored

    assert parse_factory_line("garbage") is None
    assert parse_factory_line("FACTORY|device_id=not-a-mac|fw=1") is None

    # ANSI escapes from log formatters must not pollute recorded values
    dirty = parse_factory_line("FACTORY|device_id=aa:bb:cc:dd:ee:ff|fw=1.2.3\x1b[0m")
    assert dirty is not None and dirty["fw"] == "1.2.3", dirty

    label, url = render_label("9c:13:9e:d7:7f:b8", "0123456789ABCDEF", "https://app.tuni.vn/claim")
    assert label == "TUNI 9c:13:9e:d7:7f:b8  CODE 0123456789ABCDEF"
    assert url == "https://app.tuni.vn/claim#d=9c:13:9e:d7:7f:b8&c=0123456789ABCDEF"
    assert "?" not in url  # fragment only — never a query string (spec H4)

    with tempfile.TemporaryDirectory() as tmp:
        records = Path(tmp) / "records.csv"
        digest = append_record(records, "9c:13:9e:d7:7f:b8", "p4-nano-v1.3", "B001", "1.8.9", "0123456789ABCDEF")
        content = records.read_text()
        assert "0123456789ABCDEF" not in content, "plaintext code must never be persisted"
        assert digest in content and digest == hashlib.sha256(b"0123456789ABCDEF").hexdigest()
        append_record(records, "aa:bb:cc:dd:ee:ff", None, None, None, "AAAAAAAAAAAAAAAA")
        assert len(records.read_text().splitlines()) == 3  # header + 2 rows

    print("[station] self-test OK")


# --- Main --------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="serial port (e.g. /dev/cu.usbmodemXXXX)")
    parser.add_argument("--image", help="merged factory image: releases/v*_tuni-p4.zip or merged-binary.bin")
    parser.add_argument("--api-url", default="https://robo-worker.taskfi.workers.dev")
    parser.add_argument("--claim-base-url", default="https://app.tuni.vn/claim")
    parser.add_argument("--hw-rev", default="p4-nano-v1.3")
    parser.add_argument("--batch", default=None)
    parser.add_argument("--records", default="factory-records.csv")
    parser.add_argument("--expected-chip", default="esp32-p4")
    parser.add_argument("--expected-rev", default="v1.", help="chip revision prefix guard; '' disables")
    parser.add_argument("--skip-flash", action="store_true", help="device already flashed; just capture + register")
    parser.add_argument("--mock-serial", metavar="FILE", help="read the FACTORY line from a file (no hardware)")
    parser.add_argument("--dry-run", action="store_true", help="no server write; prints what would be registered")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        self_test()
        return

    token = os.environ.get("FACTORY_STATION_TOKEN", "")
    if not token and not args.dry_run:
        sys.exit("FACTORY_STATION_TOKEN is not set (use --dry-run to rehearse without it)")

    if args.mock_serial:
        identity = factory_lines_from_file(args.mock_serial)
    else:
        if not args.port:
            sys.exit("--port is required (or use --mock-serial)")
        if not args.skip_flash:
            if not args.image:
                sys.exit("--image is required unless --skip-flash")
            image = resolve_image(args.image)
            preflight_chip(args.port, args.expected_chip, args.expected_rev)
            flash_image(args.port, image)
        identity = wait_for_factory_line(args.port)

    device_id = identity["device_id"]
    fw_version = identity.get("fw", "")
    print(f"[station] device_id={device_id} fw={fw_version}")

    if args.dry_run:
        print(f"[station] DRY RUN — would register {device_id} (hw={args.hw_rev} batch={args.batch or '-'})")
        return

    result = register_device(args.api_url, token, identity, args.hw_rev, args.batch)
    claim_code = result["claimCode"]
    append_record(args.records, device_id, args.hw_rev, args.batch, fw_version, claim_code)
    label, claim_url = render_label(device_id, claim_code, args.claim_base_url)

    print("\n================ LABEL ================")
    print(label)
    print(claim_url)
    print("=======================================\n")
    # QR PNGs contain the PLAINTEXT claim code (the QR is the physical
    # credential) — they live in a dedicated dir and must be purged after
    # printing; the bench machine must never accumulate claimable codes.
    labels_dir = Path("labels-PURGE-AFTER-PRINTING")
    labels_dir.mkdir(exist_ok=True)
    qr_path = labels_dir / f"label-{device_id.replace(':', '')}.png"
    if write_qr_png(claim_url, str(qr_path)):
        print(f"[station] QR written: {qr_path}")
        print("[station] ⚠ the PNG contains the claim code — DELETE it after printing the label")
    else:
        print("[station] qrcode package not installed — no QR PNG (pip install qrcode[pil])")
    print(f"[station] recorded in {args.records} (code hash only). Print the label NOW — the code is not stored.")


if __name__ == "__main__":
    main()
