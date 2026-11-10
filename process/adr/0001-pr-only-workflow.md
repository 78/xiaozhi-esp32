# ADR 0001 — All changes land through pull requests

| Field | Value |
| --- | --- |
| Status | accepted |
| Date | 2026-11-10 |
| Supersedes | — |

## Context

Lila is a personal fork of a large firmware project, edited in short sessions
with an AI agent. Two of the first commits went straight to `main`, which means
a failing build or a leaked credential would be immediately live for anyone
tracking the fork, with no review checkpoint and no CI record.

The repository also gained opt-in features (network tools, credentials, a build
wrapper). Those are exactly the changes where a second look — human or automated
— prevents mistakes that are expensive to revert.

## Decision

1. `main` is protected: every change arrives through a pull request from a
   `devs/<user>/<type>/<slug>` branch, admins included.
2. Pull requests must pass the required CI checks before merge.
3. Direct pushes to `main` are detected by a `Main branch guard` workflow, which
   fails and names the offending commit.
4. Force pushes to `main` and branch deletion are disabled.
5. The policy applies to process changes too: workflow, standards and roadmap
   edits are code changes.

## Consequences

**Positive**

- Every change has a spec, a diff, a CI run and a review record.
- Breaking firmware changes surface in CI before they reach a branch anyone
  builds from.
- An agent working alone still leaves an auditable trail.

**Negative**

- A one-line typo fix needs a branch and a PR (a few extra seconds).
- Branch protection must be configured once in the GitHub settings; until then
  only the detective guard is active.

**Neutral**

- Local work is unchanged: branch, commit, push, open PR.

## Enforcement

| Layer | Mechanism |
| --- | --- |
| Preventive | GitHub branch protection / ruleset on `main` (see `.github/BRANCH_PROTECTION.md`) |
| Detective | `.github/workflows/pr-guard.yml` queries the commit's associated PRs |
| Social | `process/WORKFLOW.md`, `AGENTS.md`, PR template checklist |

## Alternatives considered

- **Push directly, rely on CI afterwards.** Rejected: CI on `main` reports a
  break after it is already published, and there is no review point.
- **Feature branches without protection, merge locally.** Rejected: it skips the
  PR record that the workflow and roadmap depend on.
- **Enforce with a pre-push hook only.** Rejected: hooks are local, optional and
  bypassable with `--no-verify`.
