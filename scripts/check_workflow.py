#!/usr/bin/env python3
"""Validate the Lila development workflow.

Mechanical checks only: the process files exist and are linked, `.env` is never
tracked, secrets do not look like they were committed, the roadmap keeps the
current feature set, both skill trees agree, and the required workflows keep the
policy hooks. Run by CI and by hand before a pull request.

Exit code is 0 when everything passes and 1 when any check fails.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

REQUIRED_FILES = [
    "AGENTS.md",
    "CHANGELOG.md",
    ".env.example",
    ".github/PULL_REQUEST_TEMPLATE.md",
    ".github/BRANCH_PROTECTION.md",
    ".github/workflows/ci.yml",
    ".github/workflows/firmware-build.yml",
    ".github/workflows/pr-guard.yml",
    "openspec/config.yaml",
    "process/README.md",
    "process/WORKFLOW.md",
    "process/STANDARDS.md",
    "process/ROADMAP.md",
    "process/TOOLING.md",
    "process/specs/README.md",
    "process/specs/_template/FEAT_000.md",
    "process/specs/_template/tasks.md",
    "process/standards/firmware-cpp.md",
    "process/standards/python-tooling.md",
    "process/standards/ci-release.md",
    "process/standards/docs.md",
    "process/standards/security.md",
    "process/adr/0001-pr-only-workflow.md",
    ".opencode/skills/feature-pipeline/SKILL.md",
    ".opencode/skills/standards-injector/SKILL.md",
    ".opencode/skills/architecture-compliance-check/SKILL.md",
    ".opencode/skills/code-review-checklist/SKILL.md",
    "scripts/check_workflow.py",
    "scripts/check_format.py",
    "scripts/sync_skills.py",
]

REQUIRED_ENV_KEYS = [
    "LILA_BOARD",
    "LILA_NAME",
    "LILA_LANGUAGE",
    "LILA_WAKE_WORD",
    "LILA_DISPLAY",
    "LILA_TAVILY_API_KEY",
    "LILA_TELEGRAM_BOT_TOKEN",
    "LILA_TELEGRAM_CHAT_ID",
]

SECRET_ENV_KEYS = {
    "LILA_TAVILY_API_KEY",
    "LILA_TELEGRAM_BOT_TOKEN",
    "LILA_TELEGRAM_CHAT_ID",
}

REQUIRED_GITIGNORE = [".env", ".codegraph/"]

ROADMAP_MARKERS = [
    "scripts/build_lila.py",
    "scripts/build.py",
    "--sdkconfig",
    "web.search",
    "self.telegram.send",
    "custom:lila",
    "es-ES",
    "LCD_ST7789_240X240_7PIN",
    "wk-esp32s3-dev",
]

README_MARKERS = [
    "Development workflow",
    "process/WORKFLOW.md",
    "process/ROADMAP.md",
]

AGENTS_MARKERS = [
    "process/WORKFLOW.md",
    "process/standards/security.md",
    "Never push to `main`",
]

WORKFLOW_MARKERS = {
    ".github/workflows/ci.yml": [
        "pull_request",
        "scripts/check_workflow.py",
        "scripts/check_format.py",
        "scripts/sync_skills.py --check",
        "unittest discover",
    ],
    ".github/workflows/firmware-build.yml": [
        "workflow_dispatch",
        "espressif/idf",
        "scripts/build_lila.py",
    ],
    ".github/workflows/pr-guard.yml": [
        "push:",
        "associatedPullRequests",
    ],
}

SECRET_PATTERNS: list[tuple[str, re.Pattern[str]]] = [
    ("Telegram bot token", re.compile(r"\b\d{8,10}:[A-Za-z0-9_-]{35}\b")),
    ("Tavily API key", re.compile(r"\btvly-[A-Za-z0-9]{20,}\b")),
    ("OpenAI-style API key", re.compile(r"\bsk-[A-Za-z0-9]{20,}\b")),
    ("GitHub token", re.compile(r"\bgh[pousr]_[A-Za-z0-9]{36,}\b")),
    ("AWS access key", re.compile(r"\bAKIA[0-9A-Z]{16}\b")),
    (
        "private key block",
        re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    ),
]

SKIP_DIRS = {
    ".git",
    ".codegraph",
    ".venv",
    "venv",
    "__pycache__",
    "build",
    "components",
    "dist",
    "managed_components",
    "node_modules",
    "releases",
}

SKIP_SUFFIXES = {
    ".a",
    ".bin",
    ".bmp",
    ".dll",
    ".dylib",
    ".exe",
    ".gif",
    ".ico",
    ".jpeg",
    ".jpg",
    ".mp3",
    ".o",
    ".ogg",
    ".otf",
    ".pdf",
    ".png",
    ".pyc",
    ".so",
    ".ttf",
    ".wav",
    ".webp",
    ".woff",
    ".woff2",
    ".zip",
}

MAX_SCAN_BYTES = 2 * 1024 * 1024


def read_text(path: Path) -> str:
    """Read a text file as UTF-8, tolerating stray bytes in docs."""
    return path.read_text(encoding="utf-8", errors="replace")


def tracked_files(root: Path) -> list[str]:
    """List repository files, preferring the git index when available."""
    try:
        result = subprocess.run(
            [
                "git",
                "ls-files",
                "-z",
                "--cached",
                "--others",
                "--exclude-standard",
            ],
            cwd=root,
            capture_output=True,
            check=False,
        )
    except OSError:
        result = None
    if result is not None and result.returncode == 0:
        names = [
            name.decode("utf-8", errors="replace")
            for name in result.stdout.split(b"\0")
            if name
        ]
        return sorted(set(names))

    names = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if any(part in SKIP_DIRS for part in relative.parts):
            continue
        names.append(relative.as_posix())
    return sorted(names)


def find_secrets(text: str) -> list[tuple[str, str]]:
    """Return ``(kind, matched_line)`` for secret-shaped strings in *text*."""
    findings: list[tuple[str, str]] = []
    for kind, pattern in SECRET_PATTERNS:
        for match in pattern.finditer(text):
            line_start = text.rfind("\n", 0, match.start()) + 1
            line_end = text.find("\n", match.end())
            if line_end == -1:
                line_end = len(text)
            findings.append((kind, text[line_start:line_end].strip()))
    return findings


def scan_secrets(root: Path) -> list[str]:
    """Scan candidate text files for committed credentials."""
    problems: list[str] = []
    for name in tracked_files(root):
        path = root / name
        if not path.is_file() or path.suffix.lower() in SKIP_SUFFIXES:
            continue
        if path.stat().st_size > MAX_SCAN_BYTES:
            continue
        try:
            raw = path.read_bytes()
        except OSError:
            continue
        if b"\0" in raw[:1024]:
            continue
        for kind, line in find_secrets(raw.decode("utf-8", errors="replace")):
            problems.append(f"{name}: {kind}: {line[:120]}")
    return problems


def check_required_files(root: Path) -> list[str]:
    return [
        f"missing required file: {name}"
        for name in REQUIRED_FILES
        if not (root / name).is_file()
    ]


def check_gitignore(root: Path) -> list[str]:
    path = root / ".gitignore"
    if not path.is_file():
        return ["missing .gitignore"]
    content = read_text(path).splitlines()
    return [
        f".gitignore must ignore {entry}"
        for entry in REQUIRED_GITIGNORE
        if entry not in (line.strip() for line in content)
    ]


def check_env_example(root: Path) -> list[str]:
    path = root / ".env.example"
    if not path.is_file():
        return ["missing .env.example"]
    values: dict[str, str] = {}
    for line in read_text(path).splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        values[key.strip()] = value.strip()
    problems = [
        f".env.example is missing {key}" for key in REQUIRED_ENV_KEYS if key not in values
    ]
    problems.extend(
        f".env.example must leave {key} empty" for key in SECRET_ENV_KEYS if values.get(key)
    )
    return problems


def check_env_not_tracked(root: Path) -> list[str]:
    if ".env" in tracked_files(root):
        return [".env is tracked by git; remove it from the index"]
    return []


def check_markers(root: Path) -> list[str]:
    problems: list[str] = []
    checks = [
        ("README.md", README_MARKERS),
        ("AGENTS.md", AGENTS_MARKERS),
    ]
    checks.extend(WORKFLOW_MARKERS.items())
    for name, markers in checks:
        path = root / name
        if not path.is_file():
            problems.append(f"missing required file: {name}")
            continue
        content = read_text(path)
        for marker in markers:
            if marker not in content:
                problems.append(f"{name} must mention {marker!r}")
    roadmap = root / "process/ROADMAP.md"
    if roadmap.is_file():
        content = read_text(roadmap)
        for marker in ROADMAP_MARKERS:
            if marker not in content:
                problems.append(f"process/ROADMAP.md must list {marker!r}")
    return problems


def check_pinned_formatter(root: Path) -> list[str]:
    """The CI, the checker and the standard must pin the same clang-format."""
    script = root / "scripts" / "check_format.py"
    workflow = root / ".github" / "workflows" / "ci.yml"
    standard = root / "process" / "standards" / "firmware-cpp.md"
    if not script.is_file() or not workflow.is_file():
        return ["missing scripts/check_format.py or .github/workflows/ci.yml"]

    match = re.search(r'PINNED_VERSION\s*=\s*"([^"]+)"', read_text(script))
    if match is None:
        return ["scripts/check_format.py must declare PINNED_VERSION"]
    version = match.group(1)

    problems = []
    pinned = set(re.findall(r"clang-format==([0-9][0-9.]*)", read_text(workflow)))
    if pinned != {version}:
        problems.append(
            f".github/workflows/ci.yml must pin clang-format=={version} (found {sorted(pinned)})"
        )
    if standard.is_file() and version not in read_text(standard):
        problems.append(
            f"process/standards/firmware-cpp.md must document clang-format {version}"
        )
    return problems


def check_links(root: Path) -> list[str]:
    """Verify relative markdown links in the process documentation."""
    problems: list[str] = []
    documents = list((root / "process").rglob("*.md"))
    for name in (".github/PULL_REQUEST_TEMPLATE.md", ".github/BRANCH_PROTECTION.md"):
        path = root / name
        if path.is_file():
            documents.append(path)

    link_pattern = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
    for document in documents:
        for target in link_pattern.findall(read_text(document)):
            target = target.strip()
            if target.startswith(("http://", "https://", "mailto:", "#")):
                continue
            target = target.split("#", 1)[0]
            if not target:
                continue
            resolved = (document.parent / target).resolve()
            if not resolved.exists():
                problems.append(
                    f"{document.relative_to(root).as_posix()} links to missing {target}"
                )
    return problems


def check_skills(root: Path) -> list[str]:
    """Reuse the mirror checker so CI and the sync script agree."""
    sys.path.insert(0, str(root / "scripts"))
    try:
        from sync_skills import compare_trees
    finally:
        sys.path.pop(0)
    return [
        f"skills: {problem}"
        for problem in compare_trees(
            root / ".opencode" / "skills", root / ".claude" / "skills"
        )
    ]


def run_checks(root: Path) -> list[str]:
    problems: list[str] = []
    problems.extend(check_required_files(root))
    problems.extend(check_gitignore(root))
    problems.extend(check_env_example(root))
    problems.extend(check_env_not_tracked(root))
    problems.extend(check_markers(root))
    problems.extend(check_pinned_formatter(root))
    problems.extend(check_links(root))
    problems.extend(check_skills(root))
    problems.extend(scan_secrets(root))
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=ROOT,
        help="repository root (defaults to the parent of scripts/)",
    )
    args = parser.parse_args(argv)
    root = args.root.resolve()

    problems = run_checks(root)
    if problems:
        print(f"Workflow check failed with {len(problems)} problem(s):", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1

    print(
        "Workflow check passed: "
        f"{len(REQUIRED_FILES)} required files, links, .env policy, "
        f"roadmap, skills and secret scan."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
