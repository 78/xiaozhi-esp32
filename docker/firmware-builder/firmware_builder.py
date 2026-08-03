#!/usr/bin/env python3
"""One-shot cloud firmware build entrypoint.

The container is intentionally job-oriented: one invocation builds exactly one
board configuration, writes immutable artifacts and metadata to the output
directory, and exits with the underlying build status.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Sequence


SAFE_BOARD_DIR = re.compile(r"^[a-z0-9][a-z0-9._/-]*$")
SAFE_REPORTED_IDENTIFIER = re.compile(r"^[a-z0-9][a-z0-9.-]*$")
SAFE_JOB_ID = re.compile(r"^[a-z0-9][a-z0-9.-]*$")
SAFE_WAKE_WORD = re.compile(r"^(?:disabled|nihaoxiaozhi|wn9[sl]?_[a-z0-9_]+)$")
ARTIFACTS = {
    "ota": Path("build/xiaozhi.bin"),
    "full": Path("build/merged-binary.bin"),
}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def env(name: str) -> str | None:
    value = os.environ.get(name)
    return value.strip() if value and value.strip() else None


def env_enabled(name: str) -> bool:
    return (env(name) or "").casefold() in {"1", "true", "yes", "on"}


def oss_config() -> dict[str, str] | None:
    if not env_enabled("FIRMWARE_OSS_UPLOAD"):
        return None

    values = {
        "access_key_id": env("OSS_ACCESS_KEY_ID"),
        "access_key_secret": env("OSS_ACCESS_KEY_SECRET"),
        "bucket": env("OSS_BUCKET_NAME"),
        "endpoint": env("FIRMWARE_OSS_ENDPOINT"),
        "prefix": env("FIRMWARE_OSS_PREFIX") or "custom_firmwares",
    }
    missing = [name for name, value in values.items() if not value]
    if missing:
        raise ValueError(
            "OSS upload is enabled but configuration is missing: "
            + ", ".join(missing)
        )

    prefix = str(values["prefix"]).strip("/")
    if not prefix or ".." in Path(prefix).parts:
        raise ValueError(f"Invalid OSS prefix: {prefix!r}")
    endpoint = str(values["endpoint"])
    if not endpoint.startswith(("http://", "https://")):
        endpoint = "https://" + endpoint

    return {
        "access_key_id": str(values["access_key_id"]),
        "access_key_secret": str(values["access_key_secret"]),
        "bucket": str(values["bucket"]),
        "endpoint": endpoint,
        "prefix": prefix,
    }


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description=(
            "Build one XiaoZhi firmware board configuration. Arguments may also "
            "be supplied through FIRMWARE_BOARD_DIR, FIRMWARE_BOARD_NAME, "
            "FIRMWARE_LANGUAGE, and FIRMWARE_WAKE_WORD."
        )
    )
    result.add_argument("--board-dir", default=env("FIRMWARE_BOARD_DIR"))
    result.add_argument("--board-name", default=env("FIRMWARE_BOARD_NAME"))
    result.add_argument("--language", default=env("FIRMWARE_LANGUAGE"))
    result.add_argument("--wake-word", default=env("FIRMWARE_WAKE_WORD"))
    result.add_argument(
        "--source-dir",
        type=Path,
        default=Path(env("FIRMWARE_SOURCE_DIR") or "/opt/xiaozhi-esp32"),
    )
    result.add_argument(
        "--output-dir",
        type=Path,
        default=Path(env("FIRMWARE_OUTPUT_DIR") or "/output"),
    )
    result.add_argument(
        "--job-id",
        default=env("FIRMWARE_JOB_ID"),
        help="Optional caller job identifier written to manifest.json",
    )
    return result


def validate(args: argparse.Namespace) -> None:
    missing = [
        option
        for option, value in (
            ("--board-dir", args.board_dir),
            ("--board-name", args.board_name),
            ("--language", args.language),
            ("--wake-word", args.wake_word),
        )
        if not value
    ]
    if missing:
        raise ValueError(f"Missing required build inputs: {', '.join(missing)}")

    if (
        not SAFE_BOARD_DIR.fullmatch(args.board_dir)
        or args.board_dir.startswith("/")
        or ".." in Path(args.board_dir).parts
    ):
        raise ValueError(f"Invalid board directory: {args.board_dir!r}")
    if not SAFE_REPORTED_IDENTIFIER.fullmatch(args.board_name):
        raise ValueError(f"Invalid board name: {args.board_name!r}")
    normalized_wake_word = args.wake_word.casefold().replace("-", "_")
    if not SAFE_WAKE_WORD.fullmatch(normalized_wake_word):
        raise ValueError(f"Invalid wake word: {args.wake_word!r}")
    args.wake_word = normalized_wake_word

    build_script = args.source_dir / "scripts/build.py"
    if not build_script.is_file():
        raise ValueError(f"Firmware build script not found: {build_script}")

    config_path = (
        args.source_dir / "main" / "boards" / args.board_dir / "config.json"
    )
    if not config_path.is_file():
        raise ValueError(f"Board configuration not found: {config_path}")
    try:
        config = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"Invalid board configuration {config_path}: {error}") from error

    configured_type = config.get("type")
    if (
        not isinstance(configured_type, str)
        or not SAFE_REPORTED_IDENTIFIER.fullmatch(configured_type)
    ):
        raise ValueError(
            f"Invalid board type in {config_path}: {configured_type!r}"
        )
    args.board_type = configured_type
    configured_names = {
        build.get("name")
        for build in config.get("builds", [])
        if isinstance(build, dict)
    }
    if args.board_name not in configured_names:
        raise ValueError(
            f"Board name {args.board_name!r} is not defined for {args.board_dir}"
        )


def run_and_log(command: Sequence[str], cwd: Path, log_path: Path) -> int:
    with log_path.open("w", encoding="utf-8") as log:
        log.write(f"command={json.dumps(list(command), ensure_ascii=False)}\n")
        log.flush()
        process = subprocess.Popen(
            list(command),
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        assert process.stdout is not None
        with process.stdout:
            for line in process.stdout:
                sys.stdout.write(line)
                sys.stdout.flush()
                log.write(line)
                log.flush()
        return process.wait()


def command_output(command: Sequence[str], cwd: Path) -> str:
    try:
        return subprocess.check_output(
            list(command),
            cwd=cwd,
            text=True,
            encoding="utf-8",
            stderr=subprocess.STDOUT,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def project_version(source_dir: Path) -> str:
    content = (source_dir / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r'^set\(PROJECT_VER\s+"([^"]+)"\)', content, re.MULTILINE)
    return match.group(1) if match else "unknown"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def collect_artifacts(source_dir: Path, output_dir: Path) -> list[dict[str, object]]:
    collected: list[dict[str, object]] = []
    for kind, relative_path in ARTIFACTS.items():
        source = source_dir / relative_path
        if not source.is_file():
            raise FileNotFoundError(f"Expected build artifact not found: {source}")
        destination = output_dir / source.name
        temporary = destination.with_suffix(destination.suffix + ".tmp")
        shutil.copyfile(source, temporary)
        temporary.replace(destination)
        collected.append(
            {
                "kind": kind,
                "file": destination.name,
                "size": destination.stat().st_size,
                "sha256": sha256(destination),
            }
        )
    return collected


def write_manifest(output_dir: Path, manifest: dict[str, object]) -> None:
    path = output_dir / "manifest.json"
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def upload_outputs(
    output_dir: Path,
    manifest: dict[str, Any],
    config: dict[str, str],
) -> None:
    if not manifest.get("job_id"):
        raise ValueError("FIRMWARE_JOB_ID is required when OSS upload is enabled")

    try:
        import oss2
    except ImportError as error:
        raise RuntimeError("oss2 is required for OSS upload") from error

    job_id = str(manifest["job_id"])
    if not SAFE_JOB_ID.fullmatch(job_id):
        raise ValueError(f"Invalid job ID for OSS upload: {job_id!r}")

    base_key = f"{config['prefix']}/{job_id}"
    file_names = ["build.log"]
    file_names.extend(str(item["file"]) for item in manifest["artifacts"])
    file_names.append("manifest.json")
    object_keys = {name: f"{base_key}/{name}" for name in file_names}

    manifest["oss"] = {
        "bucket": config["bucket"],
        "endpoint": config["endpoint"],
        "prefix": base_key,
        "objects": object_keys,
    }
    manifest["delivery_status"] = "uploading"
    write_manifest(output_dir, manifest)

    auth = oss2.Auth(config["access_key_id"], config["access_key_secret"])
    bucket = oss2.Bucket(auth, config["endpoint"], config["bucket"])
    for name in file_names[:-1]:
        bucket.put_object_from_file(object_keys[name], str(output_dir / name))

    manifest["delivery_status"] = "succeeded"
    write_manifest(output_dir, manifest)
    bucket.put_object_from_file(
        object_keys["manifest.json"], str(output_dir / "manifest.json")
    )


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    started_at = utc_now()
    try:
        validate(args)
        upload_config = oss_config()
    except ValueError as error:
        print(f"firmware-builder: {error}", file=sys.stderr)
        return 2

    args.output_dir.mkdir(parents=True, exist_ok=True)
    log_path = args.output_dir / "build.log"
    manifest: dict[str, object] = {
        "schema_version": 1,
        "status": "running",
        "job_id": args.job_id,
        "board_dir": args.board_dir,
        "board_type": args.board_type,
        "board_name": args.board_name,
        "language": args.language,
        "wake_word": args.wake_word,
        "firmware_version": project_version(args.source_dir),
        "firmware_source_revision": env("FIRMWARE_SOURCE_REVISION") or "unknown",
        "idf_version": command_output(["idf.py", "--version"], args.source_dir),
        "runtime_architecture": command_output(["uname", "-m"], args.source_dir),
        "runtime_cpu_count": os.cpu_count(),
        "started_at": started_at,
        "artifacts": [],
    }
    write_manifest(args.output_dir, manifest)

    command = [
        sys.executable,
        "scripts/build.py",
        args.board_dir,
        "--name",
        args.board_name,
        "--language",
        args.language,
        "--wake-word",
        args.wake_word,
    ]
    return_code = run_and_log(command, args.source_dir, log_path)
    manifest["finished_at"] = utc_now()
    manifest["exit_code"] = return_code

    if return_code == 0:
        try:
            manifest["artifacts"] = collect_artifacts(args.source_dir, args.output_dir)
            manifest["status"] = "succeeded"
        except (OSError, FileNotFoundError) as error:
            print(f"firmware-builder: {error}", file=sys.stderr)
            manifest["status"] = "failed"
            manifest["error"] = str(error)
            return_code = 1
            manifest["exit_code"] = return_code
    else:
        manifest["status"] = "failed"

    write_manifest(args.output_dir, manifest)
    if upload_config is not None:
        try:
            upload_outputs(args.output_dir, manifest, upload_config)
        except Exception as error:
            print(f"firmware-builder: OSS upload failed: {error}", file=sys.stderr)
            manifest["status"] = "failed"
            manifest["delivery_status"] = "failed"
            manifest["error"] = f"OSS upload failed: {error}"
            return_code = 1
            manifest["exit_code"] = return_code
            write_manifest(args.output_dir, manifest)
    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
