#!/usr/bin/env python3
"""Compatibility entry point for board builds; use scripts/build.py instead."""

import sys

import build


def main():
    print(
        "Warning: scripts/release.py is deprecated; use scripts/build.py instead.",
        file=sys.stderr,
    )
    build.main()


if __name__ == "__main__":
    main()
