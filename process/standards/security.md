# Security standard

## Secrets

- **SEC-1** Credentials never enter the repository. They arrive at build time
  through `.env` (git-ignored) or `idf.py menuconfig`, and are compiled into the
  firmware by design.
- **SEC-2** `.env.example` documents keys with empty values only. Never commit a
  token, chat id, API key or private URL.
- **SEC-3** `.env` is a personal file: do not copy it into a PR, an issue, a
  screenshot or a CI artifact.
- **SEC-4** If a secret is exposed, rotate it first, then open a PR that removes
  it. History rewriting does not make a leaked token safe.

## Logging

- **SEC-5** Never log a URL, header or body that embeds a secret. Telegram bot
  URLs contain the token, so log a label (`Telegram API`) instead of the URL.
- **SEC-6** Error messages may include the HTTP status and the provider's
  description, never the request headers or the credential.
- **SEC-7** `ESP_LOGI`/`ESP_LOGE` and Python `print` must not echo full network
  payloads; truncate to a useful, non-sensitive excerpt.

## Network input

- **SEC-8** Validate status codes and parse defensively: check JSON types, bound
  array sizes, truncate long strings, and handle a missing field as "not found"
  rather than a crash.
- **SEC-9** Set an explicit HTTP timeout on every request. Requests triggered by
  a tool callback must not hang the main task.
- **SEC-10** Allow only one in-flight request for tools that hit the same
  provider, and release the guard on every exit path.

## Build and CI

- **SEC-11** CI uses the automatic `GITHUB_TOKEN` with `contents: read`. No
  repository secrets for the firmware build.
- **SEC-12** `scripts/check_workflow.py` scans tracked files for token-shaped
  strings and fails the PR. Fix the finding; do not add an allowlist entry for a
  real value.
- **SEC-13** Third-party components stay pinned through `main/idf_component.yml`;
  a dependency bump is its own PR with a note on why.

## Reporting

Open a private security advisory on the repository instead of a public issue for
anything that exposes credentials, allows remote code execution or leaks user
data. Rotate first, disclose second.
