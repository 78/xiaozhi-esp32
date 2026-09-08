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
import time
import urllib.error
import urllib.parse
import urllib.request
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
UPLOAD_MAX_ATTEMPTS = 4
UPLOAD_BASE_DELAY_SECONDS = 1
UPLOAD_TIMEOUT_SECONDS = 120


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def env(name: str) -> str | None:
    value = os.environ.get(name)
    return value.strip() if value and value.strip() else None


def upload_config() -> dict[str, str] | None:
    upload_url = env("FIRMWARE_UPLOAD_URL")
    upload_token = env("FIRMWARE_UPLOAD_TOKEN")
    if not upload_url and not upload_token:
        return None
    if not upload_url or not upload_token:
        raise ValueError(
            "FIRMWARE_UPLOAD_URL and FIRMWARE_UPLOAD_TOKEN must be configured together"
        )
    parsed_url = urllib.parse.urlparse(upload_url)
    if parsed_url.scheme not in {"http", "https"} or not parsed_url.netloc:
        raise ValueError("FIRMWARE_UPLOAD_URL must be an absolute HTTP(S) URL")
    return {"url": upload_url.rstrip("/"), "token": upload_token}


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description=(
            "Build one XiaoZhi firmware board configuration. Arguments may also "
            "be supplied through FIRMWARE_BOARD_DIR, FIRMWARE_BOARD_NAME, "
            "FIRMWARE_LANGUAGE, FIRMWARE_WAKE_WORD, and "
            "FIRMWARE_BUILD_OPTIONS."
        )
    )
    result.add_argument("--board-dir", default=env("FIRMWARE_BOARD_DIR"))
    result.add_argument("--board-name", default=env("FIRMWARE_BOARD_NAME"))
    result.add_argument("--language", default=env("FIRMWARE_LANGUAGE"))
    result.add_argument("--wake-word", default=env("FIRMWARE_WAKE_WORD"))
    result.add_argument(
        "--build-options-json",
        default=env("FIRMWARE_BUILD_OPTIONS") or "{}",
        help="Curated semantic build options as a JSON object",
    )
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

    try:
        build_options = json.loads(args.build_options_json)
    except json.JSONDecodeError as error:
        raise ValueError(f"Invalid build options JSON: {error}") from error
    if not isinstance(build_options, dict):
        raise ValueError("Build options JSON must contain an object")
    invalid_values = [
        key
        for key, value in build_options.items()
        if not isinstance(key, str) or not isinstance(value, (str, bool))
    ]
    if invalid_values:
        raise ValueError(
            "Build option values must be strings or booleans: "
            + ", ".join(map(str, invalid_values))
        )
    args.build_options = build_options
    args.build_options_json = json.dumps(
        build_options,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    )

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


def failure_summary(log_path: Path) -> str:
    """Extract a concise actionable failure from a compiler log."""
    if not log_path.is_file():
        return "Firmware build failed"
    ansi_escape = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]")
    lines = [
        ansi_escape.sub("", line).strip()
        for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines()
    ]
    meaningful = [
        line for line in lines
        if line
        and not line.startswith(("XIAOZHI_STAGE ", "XIAOZHI_SOURCE_REVISION "))
    ]
    patterns = (
        re.compile(r"(?:fatal error|error:|ValueError:|RuntimeError:|FileNotFoundError:)", re.I),
        re.compile(r"(?:Kconfig rejected|Unsupported build option|build stopped|failed with exit code)", re.I),
    )
    for pattern in patterns:
        for line in reversed(meaningful):
            if pattern.search(line):
                return line[:500]
    return meaningful[-1][:500] if meaningful else "Firmware build failed"


def is_retryable_upload_error(error: Exception) -> bool:
    if isinstance(error, urllib.error.HTTPError):
        return error.code in {408, 429} or error.code >= 500
    return isinstance(
        error,
        (urllib.error.URLError, ConnectionError, TimeoutError, OSError),
    )


def upload_file_with_retry(
    upload_url: str,
    upload_token: str,
    local_path: Path,
) -> None:
    payload = local_path.read_bytes()
    request = urllib.request.Request(
        upload_url,
        data=payload,
        method="PUT",
        headers={
            "Authorization": f"Bearer {upload_token}",
            "Content-Type": "application/octet-stream",
            "Content-Length": str(len(payload)),
            "X-Artifact-SHA256": hashlib.sha256(payload).hexdigest(),
        },
    )
    for attempt in range(1, UPLOAD_MAX_ATTEMPTS + 1):
        try:
            with urllib.request.urlopen(
                request,
                timeout=UPLOAD_TIMEOUT_SECONDS,
            ) as response:
                response.read()
            return
        except Exception as error:
            retryable = is_retryable_upload_error(error)
            if isinstance(error, urllib.error.HTTPError):
                error.close()
            if (
                attempt >= UPLOAD_MAX_ATTEMPTS
                or not retryable
            ):
                raise
            delay = UPLOAD_BASE_DELAY_SECONDS * (2 ** (attempt - 1))
            print(
                "firmware-builder: transient artifact upload failure for "
                f"{local_path.name}; retry {attempt + 1}/"
                f"{UPLOAD_MAX_ATTEMPTS} in {delay}s: {error}",
                file=sys.stderr,
            )
            time.sleep(delay)


def upload_outputs(
    output_dir: Path,
    manifest: dict[str, Any],
    config: dict[str, str],
) -> None:
    if not manifest.get("job_id"):
        raise ValueError("FIRMWARE_JOB_ID is required when artifact upload is enabled")

    job_id = str(manifest["job_id"])
    if not SAFE_JOB_ID.fullmatch(job_id):
        raise ValueError(f"Invalid job ID for artifact upload: {job_id!r}")

    file_names = ["build.log"]
    file_names.extend(str(item["file"]) for item in manifest["artifacts"])
    file_names.append("manifest.json")
    manifest["delivery_status"] = "uploading"
    write_manifest(output_dir, manifest)

    for name in file_names[:-1]:
        upload_file_with_retry(
            f"{config['url']}/{urllib.parse.quote(job_id)}/artifacts/"
            f"{urllib.parse.quote(name)}",
            config["token"],
            output_dir / name,
        )

    manifest["delivery_status"] = "succeeded"
    write_manifest(output_dir, manifest)
    upload_file_with_retry(
        f"{config['url']}/{urllib.parse.quote(job_id)}/artifacts/manifest.json",
        config["token"],
        output_dir / "manifest.json",
    )


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    started_at = utc_now()
    try:
        validate(args)
        artifact_upload_config = upload_config()
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
        "build_options": args.build_options,
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
        "--build-options-json",
        args.build_options_json,
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
        manifest["error"] = failure_summary(log_path)

    write_manifest(args.output_dir, manifest)
    if artifact_upload_config is not None:
        try:
            print("XIAOZHI_STAGE uploading", flush=True)
            upload_outputs(args.output_dir, manifest, artifact_upload_config)
        except Exception as error:
            print(f"firmware-builder: artifact upload failed: {error}", file=sys.stderr)
            manifest["status"] = "failed"
            manifest["delivery_status"] = "failed"
            manifest["error"] = f"Artifact upload failed: {error}"
            return_code = 1
            manifest["exit_code"] = return_code
            write_manifest(args.output_dir, manifest)
    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
