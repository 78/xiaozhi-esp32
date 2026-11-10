# Documentation standard

## Structure

- **DOC-1** The root `README.md` is Lila's front page: identity, credit to the
  upstream project, what Lila adds, quick start, then the upstream documentation
  kept intact below a horizontal rule.
- **DOC-2** Do not rewrite upstream sections to fit Lila. Add Lila sections and
  keep the upstream text recognisable so users can follow upstream tutorials.
- **DOC-3** Board documentation lives in `main/boards/<board>/README.md` and
  covers: hardware and wiring, build commands, configuration, optional tools, and
  known limitations.
- **DOC-4** Workflow documentation lives in `process/`. `process/README.md` is the
  entry point; every new workflow file must be linked from it.

## Writing

- **DOC-5** Lead with the action: a reader should be able to copy a command from
  the first screen of the section.
- **DOC-6** Every command shows the exact working directory assumptions and uses
  `python scripts/...` (not `./script.py`).
- **DOC-7** Prefer tables for options and checklists for gates; keep line lengths
  under ~100 characters.
- **DOC-8** Use placeholders (`<token>`, `tvly-...`) for credentials, never a
  real-looking example that could be mistaken for a leaked key.
- **DOC-9** State what is not verified yet ("hardware validation still needed")
  instead of implying the feature was tested on a device.

## Links and anchors

- **DOC-10** Use relative links inside the repository and absolute links only for
  external sites.
- **DOC-11** When renaming a file, update every reference in the same PR;
  `scripts/check_workflow.py` verifies the required links.
- **DOC-12** Keep headings stable when people may link to them; add a new heading
  instead of renaming an anchored one.

## Roadmap and changelog

- **DOC-13** Every user-visible change updates the shipped table in
  `process/ROADMAP.md` and adds a `CHANGELOG.md` entry.
- **DOC-14** The roadmap entry names the files involved, so the next contributor
  can find the implementation without a search.
- **DOC-15** Do not move planned items between releases without maintainer
  approval in the PR.
