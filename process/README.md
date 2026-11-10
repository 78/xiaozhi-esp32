# Lila process

Everything a contributor needs to change this repository safely. Lila is a fork
of [XiaoZhi AI Chatbot](https://github.com/78/xiaozhi-esp32); upstream behaviour
stays intact unless a spec says otherwise.

## Golden rules

1. **`main` is protected.** Never push to it. Every change lands through a pull
   request from a `devs/<user>/<type>/<slug>` branch.
2. **Spec first.** A non-trivial change starts as a feature spec
   (`process/specs/FEAT_XXX/`) and, when it touches more than one area, an
   OpenSpec change (`openspec/changes/<id>/`).
3. **Standards are mandatory.** Read the standard for every file type you touch
   (`process/standards/`), and apply it in the PR checklist.
4. **Upstream compatibility.** Do not alter existing boards, pins or Kconfig
   defaults to fit new hardware. Add a variant instead.
5. **Tests and docs travel with code.** A PR without tests, roadmap and
   changelog updates is incomplete.

## Map

| Path | Purpose |
| --- | --- |
| [`WORKFLOW.md`](WORKFLOW.md) | The step-by-step development pipeline, quality gates and PR rules |
| [`STANDARDS.md`](STANDARDS.md) | How standards are selected and enforced |
| [`ROADMAP.md`](ROADMAP.md) | What Lila ships today and what comes next |
| [`TOOLING.md`](TOOLING.md) | OpenSpec, CodeGraph and RTK setup and usage |
| [`specs/`](specs/) | Feature specs (`FEAT_XXX`) and their task lists |
| [`standards/`](standards/) | Firmware, Python, CI/release, docs and security rules |
| [`adr/`](adr/) | Architecture decision records |
| `.opencode/skills/` | Workflow skills used by OpenCode |
| `.claude/skills/` | Identical copies used by Claude Code |
| `.github/workflows/` | CI, PR guard and manual firmware build |
| `openspec/` | OpenSpec changes and living capability specs |

## Local commands

```bash
# everything CI runs on a pull request
python -m compileall -q scripts
python scripts/sync_skills.py --check
python scripts/check_workflow.py
python -m unittest discover -s scripts/tests -v

# inspect the Lila build command without building
python scripts/build_lila.py --dry-run

# format check on the C/C++ lines the change touched
python scripts/check_format.py --base origin/main
```

## Skills

The repository vendors four workflow skills. They are duplicated in
`.opencode/skills/` (OpenCode) and `.claude/skills/` (Claude Code) so both
agents behave the same way. `.opencode/skills` is the source of truth; edit it
and run:

```bash
python scripts/sync_skills.py --write   # mirror to .claude/skills
python scripts/sync_skills.py --check   # what CI verifies
```

| Skill | When it runs |
| --- | --- |
| `feature-pipeline` | "implement FEAT_XXX" / "build feature X" — runs the whole pipeline |
| `standards-injector` | Step 2.5 — detects changed file types and selects standards |
| `architecture-compliance-check` | Step 5.5 — verifies the AGENTS.md rules |
| `code-review-checklist` | Step 5.5 — the PR review checklist |

## Changing the process

Process changes are code changes: branch, check, PR. A policy change that
affects how everyone works (branching, gates, release) also needs an ADR in
`process/adr/` and a note in `ROADMAP.md` when it changes scope.
