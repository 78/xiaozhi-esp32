# Branch protection for `main`

The repository policy is PR-only: `main` must never be pushed to directly, and
every change arrives through a pull request that passes CI. This document is the
configuration that makes that preventive, plus the detective layer that catches a
bypass after the fact.

Policy rationale: [`process/adr/0001-pr-only-workflow.md`](../process/adr/0001-pr-only-workflow.md).
Workflow: [`process/WORKFLOW.md`](../process/WORKFLOW.md).

## Required configuration

Set this once in **Settings → Rules → Rulesets → New branch ruleset** (or
**Settings → Branches** on older GitHub plans).

| Setting | Value |
| --- | --- |
| Target branches | `main` |
| Enforcement status | Active |
| Restrict deletions | ✅ |
| Block force pushes | ✅ |
| Require a pull request before merging | ✅ |
| Required approvals | 0 (solo maintainer) or 1+ for contributors |
| Dismiss stale approvals on new commits | ✅ |
| Require review from Code Owners | optional |
| Require conversation resolution | ✅ |
| Require status checks to pass | ✅ |
| Require branches to be up to date before merging | ✅ |
| Do not allow bypassing the above settings | ✅ (no admin bypass) |

Required status checks:

| Check | Workflow |
| --- | --- |
| `Host checks` | `.github/workflows/ci.yml` |
| `clang-format (changed files)` | `.github/workflows/ci.yml` |
| `Build Boards` jobs that apply | `.github/workflows/build.yml` (upstream matrix) |

`Firmware build (manual)` is a `workflow_dispatch` workflow. It is not a merge
gate; paste its run URL in the PR when a change touches firmware.

`Main branch guard` runs after a push to `main`. It is not a merge gate because it
only makes sense once `main` has moved; it fails and names the commit when a
direct push is detected.

## Verification

After saving the ruleset:

1. Open a throwaway branch, push it, and confirm the PR shows the two required
   checks.
2. Confirm the **Merge** button is disabled while a check is red.
3. Try `git push origin HEAD:main` from a branch and confirm GitHub rejects it
   with a protected-branch error (the local command must fail; do not force it).
4. Confirm **Restrict deletions** and **Block force pushes** are active in the
   ruleset summary.

## If the policy is bypassed

The `Main branch guard` run is red and names the commit. Recovery:

1. Revert the commit on a branch and open a PR that reverts it.
2. Rotate any credential or token that was in the push before doing anything
   else (`process/standards/security.md`, `SEC-4`).
3. Note the incident in the reverted PR and, if it reveals a process gap, add an
   ADR rather than only fixing the symptom.

A history rewrite is not a substitute for rotation: a leaked token stays leaked.

## Why no pre-push hook

A local hook is optional, bypassable with `--no-verify`, and not shared with the
GitHub UI. Branch protection plus the guard workflow covers every path, including
edits made directly on github.com.
