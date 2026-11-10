#!/usr/bin/env python3
"""Mirror the repository skills to every agent that uses them.

`.opencode/skills/` is the source of truth. `.claude/skills/` is a generated
mirror so OpenCode and Claude Code behave identically. Run with `--check` in CI
and with `--write` after editing a skill.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / ".opencode" / "skills"
MIRROR = ROOT / ".claude" / "skills"


def read_tree(base: Path) -> dict[str, bytes]:
    """Return every file under *base* keyed by POSIX relative path.

    Line endings are normalised so a Windows checkout with ``core.autocrlf``
    does not report a false drift against Linux CI.
    """
    if not base.is_dir():
        return {}
    files: dict[str, bytes] = {}
    for path in sorted(base.rglob("*")):
        if path.is_file():
            data = path.read_bytes().replace(b"\r\n", b"\n")
            files[path.relative_to(base).as_posix()] = data
    return files


def compare_trees(source: Path, mirror: Path) -> list[str]:
    """Return human-readable differences between the two skill trees."""
    source_files = read_tree(source)
    mirror_files = read_tree(mirror)
    problems: list[str] = []

    if not source_files:
        problems.append(f"no skills found under {source}")

    for name in sorted(source_files.keys() - mirror_files.keys()):
        problems.append(f"missing in mirror: {name}")
    for name in sorted(mirror_files.keys() - source_files.keys()):
        problems.append(f"extra in mirror: {name}")
    for name in sorted(source_files.keys() & mirror_files.keys()):
        if source_files[name] != mirror_files[name]:
            problems.append(f"content differs: {name}")
    return problems


def write_mirror(source: Path, mirror: Path) -> list[str]:
    """Replace *mirror* with an exact copy of *source* and return actions."""
    source_files = read_tree(source)
    mirror_files = read_tree(mirror)
    actions: list[str] = []

    if not source_files:
        raise SystemExit(f"No skills found under {source}; nothing to mirror.")

    for name in sorted(mirror_files.keys() - source_files.keys()):
        target = mirror / name
        target.unlink()
        actions.append(f"removed {name}")

    for name, data in source_files.items():
        target = mirror / name
        if mirror_files.get(name) == data:
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        actions.append(f"wrote {name}")

    for directory in sorted(
        (p for p in mirror.rglob("*") if p.is_dir()), reverse=True
    ):
        if not any(directory.iterdir()):
            directory.rmdir()

    return actions


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--check",
        action="store_true",
        help="fail when .claude/skills differs from .opencode/skills",
    )
    group.add_argument(
        "--write",
        action="store_true",
        help="regenerate .claude/skills from .opencode/skills",
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=SOURCE,
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--mirror",
        type=Path,
        default=MIRROR,
        help=argparse.SUPPRESS,
    )
    args = parser.parse_args(argv)

    if args.write:
        actions = write_mirror(args.source, args.mirror)
        if actions:
            for action in actions:
                print(action)
        else:
            print("Skills mirror already up to date.")
        return 0

    problems = compare_trees(args.source, args.mirror)
    if problems:
        print("Skills mirror is out of date:", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        print(
            "Run `python scripts/sync_skills.py --write` and commit the mirror.",
            file=sys.stderr,
        )
        return 1
    print("Skills mirror is up to date.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
