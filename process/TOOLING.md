# Tooling

Three tools support the workflow. All of them are optional for reading the
repository, but the workflow assumes them when an agent is involved.

## OpenSpec — spec-driven development

Installed globally (`openspec` 1.14.3) and initialised in `openspec/`.

| Command | Use |
| --- | --- |
| `openspec list` | List active changes |
| `openspec list --specs` | List living capability specs |
| `openspec change new <id>` | Start a change (proposal, design, spec deltas, tasks) |
| `openspec archive <id>` | Merge a completed change into `openspec/specs/` |
| `openspec doctor` | Check the spec/change relationship health |
| `openspec context` | Print the working context for the resolved root |

Rules:

- A change id is `FEAT-XXX-<slug>` and matches the folder in `process/specs/`.
- `openspec/changes/<id>/` is committed while the work is open;
  `openspec/changes/archive/` keeps the history.
- `openspec/specs/<capability>/spec.md` is the living contract. Update it when
  behaviour changes, not only when a change is archived.
- The `--tools none` initialisation keeps agent files out of the tree; the
  workflow skills cover that role instead.

## CodeGraph — code intelligence

`@colbymchenry/codegraph` (installed globally) indexes the repository so an agent
can find symbols, call paths and blast radius without reading whole files.

| Command | Use |
| --- | --- |
| `codegraph init` | Index the repository once; creates `.codegraph/` (git-ignored) |
| `codegraph sync` | Update the index after edits |
| `codegraph status` | Index freshness and statistics |
| `codegraph query <symbol>` | Find a symbol |
| `codegraph explore <area>` | Relevant symbols plus call paths in one shot |
| `codegraph context <task>` | Build context for a specific task |
| `codegraph node <symbol>` | One symbol with caller/callee trail |
| `codegraph callers <symbol>` / `callees <symbol>` | Direct relationships |
| `codegraph impact <symbol>` | What a change affects |
| `codegraph files` | Project structure from the index |

Use it before touching shared code (`application.*`, `mcp_server.*`,
`boards/common/`), and include the `impact` result in the PR when a symbol has
many callers.

`.codegraph/` is local state: never commit it, never delete it as part of a
change.

## RTK — Rust Token Killer

[rtk-ai/rtk](https://github.com/rtk-ai/rtk) is a local CLI proxy that compresses
command output (tests, diffs, logs, builds) before an agent reads it, typically
60-90% fewer tokens for those commands. It never blocks: if a filter fails the
raw output is returned and the exit code is preserved.

Install on Windows:

```powershell
winget install rtk-ai.rtk
```

On macOS/Linux:

```bash
brew install rtk
```

Then register the hook for the agent you use:

```bash
rtk init -g                 # Claude Code (global hook)
rtk init -g --opencode      # OpenCode plugin (requires OpenCode >= 1.18.29)
```

> Check `opencode --version` first. This machine has OpenCode 1.18.26, so upgrade
> before using `--opencode`; the Claude Code hook works today.

Everyday commands:

```bash
rtk gain                 # savings dashboard for this machine
rtk gain --history       # recent commands and their savings
rtk discover             # find commands that are not going through RTK
rtk bypass git log -20   # run one command unfiltered, still tracked
```

Privacy: RTK collects no command or file contents, and telemetry is off by
default. Do not enable it for a repository that handles customer data without
checking the policy first.

## The recommended loop

1. `openspec list` / read the FEAT spec — know the intent.
2. `codegraph explore "<area>"` and `codegraph impact <symbol>` — know the code.
3. Edit, then run checks through RTK so the agent reads compact output
   (`rtk pytest`, `rtk git diff`).
4. `python scripts/check_workflow.py` and the test suite before the PR.
5. Open the PR; CI repeats the checks without any of these local tools.

None of the three tools is required to build the firmware. `scripts/build.py`
and `scripts/build_lila.py` remain the only build entry points.
