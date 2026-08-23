#!/usr/bin/env python3
"""Create an auditable Kevin Box 2 local-firmware release candidate."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import shutil


ARTIFACTS = {
    "xiaozhi.bin": ("xiaozhi.bin", "0x20000", "OTA application image"),
    "generated_assets.bin": ("generated_assets.bin", "0x800000", "WakeNet/MultiNet assets"),
    "merged-binary.bin": ("merged-binary.bin", "0x0", "Full flash image; overwrites NVS"),
    "bootloader.bin": ("bootloader/bootloader.bin", "0x0", "Bootloader"),
    "partition-table.bin": (
        "partition_table/partition-table.bin",
        "0x8000",
        "Partition table",
    ),
    "ota_data_initial.bin": ("ota_data_initial.bin", "0xd000", "Initial OTA metadata"),
}
PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_README = PROJECT_ROOT / "docs/kevin-box2-local.md"


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def package_release(
    build_dir: pathlib.Path,
    output_dir: pathlib.Path,
    release_name: str,
    source_commit: str,
    upstream_commit: str,
    idf_version: str,
    validation_status: str,
    dependencies_lock: pathlib.Path | None = None,
    readme: pathlib.Path = DEFAULT_README,
) -> dict:
    if output_dir.exists() and any(output_dir.iterdir()):
        raise ValueError(f"Output directory is not empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    manifest_artifacts = []
    for output_name, (relative_source, flash_offset, purpose) in ARTIFACTS.items():
        source = build_dir / relative_source
        if not source.is_file():
            raise ValueError(f"Required build artifact is missing: {source}")
        destination = output_dir / output_name
        shutil.copy2(source, destination)
        manifest_artifacts.append(
            {
                "name": output_name,
                "purpose": purpose,
                "flash_offset": flash_offset,
                "size": destination.stat().st_size,
                "sha256": sha256(destination),
            }
        )

    dependencies_lock = dependencies_lock or build_dir.parent / "dependencies.lock"
    if not dependencies_lock.is_file():
        raise ValueError(f"Dependency lock is missing: {dependencies_lock}")
    if not readme.is_file():
        raise ValueError(f"Release documentation is missing: {readme}")
    shutil.copy2(dependencies_lock, output_dir / "dependencies.lock")
    shutil.copy2(readme, output_dir / "README.md")

    manifest = {
        "release": release_name,
        "validation_status": validation_status,
        "board": "kevin-box-2",
        "source_commit": source_commit,
        "upstream_commit": upstream_commit,
        "esp_idf": idf_version,
        "dependencies": "dependencies.lock",
        "documentation": "README.md",
        "artifacts": manifest_artifacts,
    }
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    checksum_lines = [
        f"{artifact['sha256']}  {artifact['name']}" for artifact in manifest_artifacts
    ]
    checksum_lines.append(f"{sha256(output_dir / 'manifest.json')}  manifest.json")
    checksum_lines.append(f"{sha256(output_dir / 'dependencies.lock')}  dependencies.lock")
    checksum_lines.append(f"{sha256(output_dir / 'README.md')}  README.md")
    (output_dir / "SHA256SUMS").write_text("\n".join(checksum_lines) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=pathlib.Path, required=True)
    parser.add_argument("--output-dir", type=pathlib.Path, required=True)
    parser.add_argument("--release", required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--upstream-commit", required=True)
    parser.add_argument("--idf-version", default="6.0.2")
    parser.add_argument("--dependencies-lock", type=pathlib.Path)
    parser.add_argument("--readme", type=pathlib.Path, default=DEFAULT_README)
    parser.add_argument(
        "--validation-status",
        choices=("hardware-validation-pending", "hardware-validated"),
        default="hardware-validation-pending",
    )
    args = parser.parse_args()
    try:
        manifest = package_release(
            args.build_dir,
            args.output_dir,
            args.release,
            args.source_commit,
            args.upstream_commit,
            args.idf_version,
            args.validation_status,
            args.dependencies_lock,
            args.readme,
        )
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(
        f"Packaged {manifest['release']} ({manifest['validation_status']}) "
        f"to {args.output_dir}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
