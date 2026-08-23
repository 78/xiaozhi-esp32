#!/usr/bin/env python3
"""Validate built artifacts against the exact ESP partition table used by a build."""

from __future__ import annotations

import argparse
import dataclasses
import pathlib
import struct
from collections.abc import Iterable


PARTITION_MAGIC = 0x50AA
MD5_MAGIC = 0xEBEB
ENTRY = struct.Struct("<HBBLL16sL")


@dataclasses.dataclass(frozen=True)
class Partition:
    label: str
    offset: int
    size: int


def read_partition_table(path: pathlib.Path) -> dict[str, Partition]:
    data = path.read_bytes()
    partitions: dict[str, Partition] = {}
    for entry_offset in range(0, len(data) - ENTRY.size + 1, ENTRY.size):
        magic, _type, _subtype, offset, size, raw_label, _flags = ENTRY.unpack_from(
            data, entry_offset
        )
        if magic == 0xFFFF or magic == MD5_MAGIC:
            break
        if magic != PARTITION_MAGIC:
            raise ValueError(
                f"Invalid partition entry magic 0x{magic:04x} at 0x{entry_offset:x}"
            )
        label = raw_label.split(b"\0", 1)[0].decode("ascii")
        if not label:
            raise ValueError(f"Partition at 0x{entry_offset:x} has no label")
        if label in partitions:
            raise ValueError(f"Duplicate partition label: {label}")
        partitions[label] = Partition(label=label, offset=offset, size=size)
    if not partitions:
        raise ValueError(f"No partitions found in {path}")
    return partitions


def validate_artifacts(
    partitions: dict[str, Partition], artifacts: Iterable[tuple[str, pathlib.Path]]
) -> list[str]:
    reports: list[str] = []
    for label, artifact in artifacts:
        partition = partitions.get(label)
        if partition is None:
            raise ValueError(f"Partition not found: {label}")
        if not artifact.is_file():
            raise ValueError(f"Artifact not found: {artifact}")
        artifact_size = artifact.stat().st_size
        if artifact_size > partition.size:
            raise ValueError(
                f"{artifact} is 0x{artifact_size:x} bytes, exceeding {label} "
                f"partition size 0x{partition.size:x}"
            )
        free = partition.size - artifact_size
        reports.append(
            f"{label}: artifact=0x{artifact_size:x}, partition=0x{partition.size:x}, "
            f"free=0x{free:x} ({free * 100 // partition.size}%)"
        )
    return reports


def parse_artifact(value: str) -> tuple[str, pathlib.Path]:
    label, separator, path = value.partition("=")
    if not separator or not label or not path:
        raise argparse.ArgumentTypeError("artifact must use PARTITION=PATH")
    return label, pathlib.Path(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--partition-table", required=True, type=pathlib.Path, help="partition-table.bin"
    )
    parser.add_argument(
        "--artifact",
        action="append",
        required=True,
        type=parse_artifact,
        help="artifact mapping such as ota_0=build/xiaozhi.bin",
    )
    args = parser.parse_args()

    try:
        partitions = read_partition_table(args.partition_table)
        reports = validate_artifacts(partitions, args.artifact)
    except (OSError, UnicodeDecodeError, ValueError) as error:
        parser.error(str(error))
    for report in reports:
        print(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
