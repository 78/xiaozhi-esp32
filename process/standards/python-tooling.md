# Python tooling standard

Applies to `scripts/**` and `scripts/tests/**`.

## Structure

- **PY-1** `scripts/build.py` is the canonical build entry point. Wrappers
  (`scripts/build_lila.py`) only translate configuration into its CLI; they do
  not duplicate board logic.
- **PY-2** New user-facing behaviour gets a flag with a `--dry-run` mode when it
  mutates the tree or starts a build.
- **PY-3** Scripts use the standard library only. CI must never need a package
  that is not installed by the workflow.
- **PY-4** Python 3.10+ syntax is allowed (`dict[str, str]`, `X | None`); keep
  the module importable on Windows and Linux.

## Configuration

- **PY-5** Configuration precedence is: process environment, then `.env` file,
  then code default. Document the order next to the loader.
- **PY-6** Parse `.env` locally; do not add a dependency for it. Ignore blanks
  and comments, strip one level of surrounding quotes.
- **PY-7** Never print or commit a secret. Error messages name the key, not its
  value. Reject quotes and newlines in values that will be written into
  `sdkconfig`.
- **PY-8** Validate `CONFIG_KEY=VALUE` assignments before forwarding them, and
  apply raw overrides after derived options so they win deterministically.

## Errors and output

- **PY-9** User errors raise `SystemExit` with a message that says what to do,
  for example `LILA_DISPLAY='x' is invalid; use an LCD_* or OLED_* symbol`.
- **PY-10** Print the exact command before running a build (`--dry-run` output is
  the same string), so failures are reproducible.
- **PY-11** Pass commands as argument lists; never `shell=True` and never
  interpolate unescaped values into a shell string.

## Tests

- **PY-12** Tests use `unittest`, run offline, and write only inside a temporary
  directory.
- **PY-13** Always pass `encoding="utf-8"` when reading or writing text; the
  suite must pass on a Windows cp1252 console and on Linux.
- **PY-14** Build tests mock `build_board` or the process boundary; no test may
  require ESP-IDF, a device or network access.
- **PY-15** Each new flag or branch gets a test for the success path and at least
  one rejection path.

## Repository hygiene

- **PY-16** Keep helper scripts out of `main/`; tooling stays under `scripts/`.
- **PY-17** `python -m compileall -q scripts` and the full suite must pass before
  a PR.
- **PY-18** When a script changes repository policy (workflow, skills, CI), update
  `process/` docs in the same PR.
