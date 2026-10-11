#!/usr/bin/env python3
"""Prepare, verify, and publish the complete firmware release matrix."""

import argparse
import hashlib
import json
import re
import struct
import subprocess
import zipfile
from pathlib import Path

import build


CHIP_IDS = {
    "esp32": 0x00, "esp32c3": 0x05, "esp32s3": 0x09,
    "esp32c6": 0x0D, "esp32p4": 0x12, "esp32c5": 0x17,
    "esp32s31": 0x20,
}


def gh_json(repository, endpoint, optional=False):
    result = subprocess.run(
        ["gh", "api", f"repos/{repository}/{endpoint}"],
        capture_output=True, text=True,
    )
    if optional and result.returncode and "HTTP 404" in result.stderr:
        return None
    if result.returncode:
        raise RuntimeError(result.stderr.strip())
    return json.loads(result.stdout)


def preflight(repository, manifest):
    """Refuse to replace a published release or move an existing tag."""
    tag, sha = manifest["tag"], manifest["source_sha"]
    release = gh_json(repository, f"releases/tags/{tag}", optional=True)
    if release is not None:
        if not release["draft"]:
            raise ValueError(f"{tag} is already published")
        if release["target_commitish"] != sha:
            raise ValueError(f"Existing draft {tag} targets a different commit")
    ref = gh_json(repository, f"git/ref/tags/{tag}", optional=True)
    if ref:
        obj = ref["object"]
        while obj["type"] == "tag":
            obj = gh_json(repository, f"git/tags/{obj['sha']}")["object"]
        if obj["type"] != "commit" or obj["sha"] != sha:
            raise ValueError(f"Existing tag {tag} targets a different commit")
    return release


def create_manifest(version, source_sha, idf_version):
    if not version or not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("PROJECT_VER must be a numeric x.y.z release version")
    if not re.fullmatch(r"[a-f0-9]{40}", source_sha):
        raise ValueError("Release source must be a complete commit SHA")
    if idf_version < (6, 1, 0):
        raise ValueError("Full releases require ESP-IDF 6.1 or later")
    variants = build._collect_variants(idf_version=idf_version)
    if not variants or len(variants) > 256:
        raise ValueError("Release matrix must contain between 1 and 256 variants")
    matrix = [{key: v[key] for key in ("board", "name", "full_name", "target")}
              for v in variants]
    return {
        "schema_version": 1,
        "version": version,
        "tag": f"v{version}",
        "source_sha": source_sha,
        "idf_version": ".".join(map(str, idf_version)),
        "board_count": len({v["board"] for v in matrix}),
        "firmware_count": len(matrix),
        "variants": matrix,
    }


def firmware_metadata(data):
    """Read the application descriptor from the merged image's app partition."""
    if len(data) < 0x9000:
        raise ValueError("Merged firmware is truncated")
    for pos in range(0x8000, 0x9000, 32):
        if data[pos:pos + 2] != b"\xaa\x50" or data[pos + 2] != 0:
            continue
        offset, capacity = struct.unpack_from("<II", data, pos + 4)
        if offset + 32 + 256 > len(data) or capacity < 288:
            raise ValueError("Application partition is truncated")
        if data[offset] != 0xE9:
            raise ValueError("Invalid application image")
        descriptor = data[offset + 32:offset + 32 + 256]
        if struct.unpack_from("<I", descriptor)[0] != 0xABCD5432:
            raise ValueError("Invalid application descriptor")
        def string(start, end):
            return descriptor[start:end].split(b"\0", 1)[0].decode("utf-8")
        return {
            "version": string(16, 48),
            "project": string(48, 80),
            "idf_version": string(112, 144),
            "chip_id": struct.unpack_from("<H", data, offset + 12)[0],
        }
    raise ValueError("Application partition is missing")


def verify_assets(manifest, assets_dir):
    expected = {f"{manifest['tag']}_{v['full_name']}.zip": v
                for v in manifest["variants"]}
    actual = {p.name for p in assets_dir.iterdir() if p.is_file()}
    if actual != set(expected):
        raise ValueError(
            f"Firmware set mismatch; missing={sorted(set(expected) - actual)}, "
            f"unexpected={sorted(actual - set(expected))}"
        )
    if len(expected) != manifest["firmware_count"]:
        raise ValueError("Manifest firmware count does not match its variants")
    sdk = tuple(map(int, manifest["idf_version"].split(".")))[:2]
    checksums = []
    for name, variant in sorted(expected.items()):
        path = assets_dir / name
        with zipfile.ZipFile(path) as archive:
            if archive.namelist() != ["merged-binary.bin"]:
                raise ValueError(f"{name}: expected only merged-binary.bin")
            data = archive.read("merged-binary.bin")  # Also verifies ZIP CRC.
        meta = firmware_metadata(data)
        if meta["version"] != manifest["version"] or meta["project"] != "xiaozhi":
            raise ValueError(f"{name}: application version/project mismatch: {meta}")
        sdk_match = re.match(r"v?(\d+)\.(\d+)", meta["idf_version"])
        if not sdk_match or tuple(map(int, sdk_match.groups())) != sdk:
            raise ValueError(f"{name}: ESP-IDF version mismatch: {meta['idf_version']}")
        if meta["chip_id"] != CHIP_IDS[variant["target"]]:
            raise ValueError(f"{name}: chip target mismatch")
        checksums.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {name}\n")
    return "".join(checksums)


def publish(repository, manifest, assets_dir, metadata_dir, run_url, make_public):
    preflight(repository, manifest)
    checksums = verify_assets(manifest, assets_dir)
    metadata_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = metadata_dir / "release-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    checksum_path = metadata_dir / "SHA256SUMS"
    checksum_path.write_text(checksums, encoding="utf-8")
    latest = gh_json(repository, "releases/latest", optional=True)
    args = ["gh", "api", f"repos/{repository}/releases/generate-notes", "-X", "POST",
            "-f", f"tag_name={manifest['tag']}", "-f", f"target_commitish={manifest['source_sha']}"]
    if latest:
        args += ["-f", f"previous_tag_name={latest['tag_name']}"]
    generated = json.loads(subprocess.check_output(args, text=True))["body"]
    notes_path = metadata_dir / "release-notes.md"
    notes_path.write_text(
        f"{manifest['firmware_count']} firmware variants for {manifest['board_count']} board directories, "
        f"built with ESP-IDF {manifest['idf_version']}.\n\n"
        f"Source: `{manifest['source_sha']}`. [Build and validation]({run_url}).\n\n"
        "Each firmware ZIP contains `merged-binary.bin`. SHA-256 checksums and the complete "
        "variant manifest are attached. Builds and package checks do not replace physical "
        "hardware validation.\n\n" + generated + "\n", encoding="utf-8",
    )
    release = preflight(repository, manifest)
    files = sorted(assets_dir.glob("*.zip")) + [manifest_path, checksum_path]
    expected = {p.name: p for p in files}
    if release:
        extra = {a["name"] for a in release["assets"]} - set(expected)
        if extra:
            raise ValueError(f"Existing draft has unrelated assets: {sorted(extra)}")
    else:
        subprocess.run([
            "gh", "release", "create", manifest["tag"], "--repo", repository,
            "--target", manifest["source_sha"], "--draft", "--title", manifest["tag"],
            "--notes-file", str(notes_path),
        ], check=True)
    subprocess.run([
        "gh", "release", "edit", manifest["tag"], "--repo", repository,
        "--notes-file", str(notes_path),
    ], check=True)
    # Upload bounded batches so a failed upload can be resumed on the same draft.
    for start in range(0, len(files), 20):
        subprocess.run([
            "gh", "release", "upload", manifest["tag"], "--repo", repository,
            "--clobber", *map(str, files[start:start + 20]),
        ], check=True)
    remote = gh_json(repository, f"releases/tags/{manifest['tag']}")
    if {a["name"] for a in remote["assets"]} != set(expected):
        raise ValueError("Uploaded release asset set is incomplete")
    for asset in remote["assets"]:
        path = expected[asset["name"]]
        if asset["size"] != path.stat().st_size:
            raise ValueError(f"Uploaded asset size mismatch: {path.name}")
        digest = asset.get("digest")
        if digest and digest != "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest():
            raise ValueError(f"Uploaded asset digest mismatch: {path.name}")
    if make_public:
        subprocess.run([
            "gh", "release", "edit", manifest["tag"], "--repo", repository,
            "--draft=false", "--latest",
        ], check=True)
    print(f"{'Published' if make_public else 'Prepared draft'} {manifest['tag']}: {remote['html_url']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    prepare = sub.add_parser("prepare")
    prepare.add_argument("--idf-version", required=True)
    prepare.add_argument("--source-sha", required=True)
    prepare.add_argument("--output-dir", type=Path, required=True)
    prepare.add_argument("--repository")
    release = sub.add_parser("publish")
    release.add_argument("--manifest", type=Path, required=True)
    release.add_argument("--assets-dir", type=Path, required=True)
    release.add_argument("--metadata-dir", type=Path, required=True)
    release.add_argument("--repository", required=True)
    release.add_argument("--run-url", required=True)
    release.add_argument("--make-public", action="store_true")
    args = parser.parse_args()
    if args.command == "prepare":
        manifest = create_manifest(build.get_project_version(), args.source_sha,
                                   tuple(map(int, args.idf_version.split("."))))
        if args.repository:
            preflight(args.repository, manifest)
        args.output_dir.mkdir(parents=True, exist_ok=True)
        (args.output_dir / "release-manifest.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8",
        )
        print(f"version={manifest['version']}")
        print(f"tag={manifest['tag']}")
        print(f"firmware_count={manifest['firmware_count']}")
        print("variants=" + json.dumps(manifest["variants"], separators=(",", ":")))
    else:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        publish(args.repository, manifest, args.assets_dir, args.metadata_dir,
                args.run_url, args.make_public)


if __name__ == "__main__":
    main()
