# Feature specs

One folder per feature: `process/specs/FEAT_XXX/FEAT_XXX.md` plus `tasks.md`.
Copy `_template/` to start. A spec is a working document — refine it while
implementing and leave it accurate when the PR merges.

## When a spec is required

| Change | Spec |
| --- | --- |
| New board, tool, Kconfig option, protocol or build CLI change | Required |
| Behaviour change in `main/` | Required |
| Refactor with no behaviour change | Optional (state it in the PR) |
| Typo, dependency bump, CI-only tweak | Not needed |

## Numbering

- `FEAT_001`, `FEAT_002`, … zero-padded, never reused.
- The title is short and imperative: `FEAT_003 — Telegram photo sending`.
- The OpenSpec change, when used, is `FEAT-XXX-<slug>` and links back to the
  folder.

## Content rules

- Acceptance criteria must be observable ("wake word `lila` triggers a session",
  not "wake word works").
- The technical blueprint names exact files, symbols and Kconfig options.
- Scope lists what is explicitly out of scope, to keep PRs small.
- Hardware validation states what cannot be tested on the host.
- `tasks.md` stays checkable; every task is one reviewable commit or less.

## Status values

`draft` → `ready` → `in progress` → `review` → `done`, or `cancelled` with a
reason. Update the metadata line at the top of the spec in the same commit as
the state change.
