# Standards

Standards are small, enforceable rules per change area. They exist so reviews
are about design, not style, and so an agent can be held to the same bar as a
human.

## How standards are applied

1. **Detect the area.** The `standards-injector` skill maps the changed files to
   the table below.
2. **Read the standard before editing.** Each document states rules with a short
   rationale and, where useful, an example.
3. **Prove compliance.** The PR checklist has one line per applicable standard.
   `check_workflow.py` and CI enforce the mechanical parts.

| Area | Standard | Enforced by |
| --- | --- | --- |
| C/C++ firmware, boards, Kconfig, CMake | [`standards/firmware-cpp.md`](standards/firmware-cpp.md) | `architecture-compliance-check`, `clang-format`, review |
| Python scripts and build tooling | [`standards/python-tooling.md`](standards/python-tooling.md) | `compileall`, unit tests, review |
| CI, branching, releases, versioning | [`standards/ci-release.md`](standards/ci-release.md) | CI itself, PR guard |
| Markdown, READMEs, docs | [`standards/docs.md`](standards/docs.md) | `check_workflow.py`, review |
| Secrets, logging, network input | [`standards/security.md`](standards/security.md) | `check_workflow.py`, manual review |

## Precedence

1. Platform constraints (ESP-IDF, ESP-SR, chip errata) win over everything else.
2. Upstream XiaoZhi conventions win over Lila preferences; Lila only adds.
3. A standard in this folder wins over personal preference.
4. An ADR in `process/adr/` can override a standard; update the standard in the
   same PR.

## Adding or changing a standard

- Open a PR with the standard edit and a one-paragraph justification.
- If the change affects how people work (branching, gates, releases), add an ADR.
- Standards must be checkable: "prefer readable code" is not a rule, "register
  custom tools in `InitializeTools()`" is.
