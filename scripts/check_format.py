#!/usr/bin/env python3
"""Check clang-format only on the C/C++ lines a change touched.

The repository keeps upstream formatting inside files it patches, so a
whole-file check would fail on pre-existing style. This script takes the base
and head revisions, extracts the added or modified line ranges from the diff,
and asks `clang-format --dry-run -Werror` to validate exactly those ranges.

CI pins clang-format (see `.github/workflows/ci.yml`); contributors install the
same version with `python -m pip install clang-format==23.1.5`.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

C_CPP_PATHS = [
    "*.c",
    "*.cc",
    "*.cpp",
    "*.cxx",
    "*.h",
    "*.hh",
    "*.hpp",
    "*.hxx",
]

HUNK_HEADER = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")

PINNED_VERSION = "23.1.5"


def changed_spans(root: Path, base: str, head: str) -> dict[str, list[tuple[int, int]]]:
    """Return ``{posix path: [(first, last), ...]}`` for added/modified lines."""
    diff = subprocess.run(
        [
            "git",
            "diff",
            "--unified=0",
            "--no-color",
            "--diff-filter=ACMR",
            f"{base}...{head}",
            "--",
            *C_CPP_PATHS,
        ],
        cwd=root,
        capture_output=True,
        check=False,
    )
    if diff.returncode != 0:
        raise SystemExit(f"git diff failed: {diff.stderr.decode(errors='replace').strip()}")

    spans: dict[str, list[tuple[int, int]]] = {}
    current: str | None = None
    for line in diff.stdout.decode("utf-8", errors="replace").splitlines():
        if line.startswith("+++ b/"):
            current = line[len("+++ b/"):]
            spans.setdefault(current, [])
        elif line.startswith("+++ "):
            current = None
        else:
            match = HUNK_HEADER.match(line)
            if match is None or current is None:
                continue
            start = int(match.group(1))
            count = int(match.group(2) or 1)
            if count > 0:
                spans[current].append((start, start + count - 1))
    return {path: merge_spans(ranges) for path, ranges in spans.items() if ranges}


def merge_spans(ranges: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Merge touching or overlapping ranges so clang-format gets stable input."""
    merged: list[list[int]] = []
    for start, end in sorted(ranges):
        if merged and start <= merged[-1][1] + 1:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([start, end])
    return [(start, end) for start, end in merged]


def check_file(root: Path, name: str, spans: list[tuple[int, int]], binary: str) -> str | None:
    """Run clang-format over *spans*; return the failure text or None."""
    arguments = [binary, "--dry-run", "-Werror"]
    for start, end in spans:
        arguments.append(f"--lines={start}:{end}")
    arguments.append(name)
    try:
        result = subprocess.run(
            arguments,
            cwd=root,
            capture_output=True,
            check=False,
        )
    except OSError as error:
        return f"cannot run {binary}: {error}"
    if result.returncode == 0:
        return None
    output = result.stderr.decode("utf-8", errors="replace").strip()
    lines = [line for line in output.splitlines() if line.strip()]
    return "\n".join(lines[-12:])


def resolve_binary(requested: str) -> str:
    binary = shutil.which(requested)
    if binary is None:
        raise SystemExit(
            f"clang-format was not found ({requested!r}). Install the pinned "
            f"version with: python -m pip install clang-format=={PINNED_VERSION}"
        )
    return binary


def report_version(binary: str, root: Path) -> str:
    result = subprocess.run(
        [binary, "--version"],
        cwd=root,
        capture_output=True,
        check=False,
    )
    return result.stdout.decode("utf-8", errors="replace").strip()


def run_checks(root: Path, base: str, head: str, binary: str) -> list[str]:
    spans_by_file = changed_spans(root, base, head)
    if not spans_by_file:
        return []

    failures: list[str] = []
    for name in sorted(spans_by_file):
        if not (root / name).is_file():
            failures.append(f"{name}: file missing at HEAD")
            continue
        failure = check_file(root, name, spans_by_file[name], binary)
        if failure is not None:
            ranges = ", ".join(f"{start}-{end}" for start, end in spans_by_file[name])
            failures.append(f"{name} (lines {ranges}):\n{failure}")
    return failures


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, help="merge-base revision to diff against")
    parser.add_argument("--head", default="HEAD", help="revision to check (default HEAD)")
    parser.add_argument(
        "--clang-format",
        default="clang-format",
        help="clang-format binary (default: clang-format on PATH)",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=ROOT,
        help="repository root (defaults to the parent of scripts/)",
    )
    args = parser.parse_args(argv)
    root = args.root.resolve()

    binary = resolve_binary(args.clang_format)
    print(report_version(binary, root))

    failures = run_checks(root, args.base, args.head, binary)
    if failures:
        print("clang-format check failed on changed lines:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print("clang-format check passed on changed lines.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
