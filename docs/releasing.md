# Publishing firmware releases

The **Release Firmware** workflow (`.github/workflows/release.yml`) is started
manually. It builds every release variant supported by ESP-IDF 6.1 using
`scripts/build.py <board> --name <variant> --zip`. It does not use the incremental
CI selection or include CI-only option variants.

The release version comes from `PROJECT_VER` in the root `CMakeLists.txt`.
Update that value before publishing another version. The workflow fixes every
checkout to the triggering commit and records the commit, SDK, board directories,
and full variant list in `release-manifest.json`.

## Running the workflow

Merge the workflow into the default branch first so GitHub exposes its manual
trigger. In **Actions → Release Firmware → Run workflow**, select the default
branch and leave **Publish as the latest release** enabled to publish publicly.
Disable it to build and upload an unpublished draft instead.

The equivalent commands are:

```sh
# Build all variants, upload all packages, and publish the latest release.
gh workflow run release.yml --repo 78/xiaozhi-esp32 --ref main -f publish=true

# Build all variants and leave the resulting release as a draft.
gh workflow run release.yml --repo 78/xiaozhi-esp32 --ref main -f publish=false
```

Public releases require the default branch. The workflow uses only its built-in
`GITHUB_TOKEN`; no personal access token or local ESP-IDF installation is needed.
It publishes a GitHub Release, without modifying the OTA service or OSS storage.

## What is uploaded

Each variant produces `v<version>_<full_name>.zip`, containing one
`merged-binary.bin`, matching the format used by v2.5.0. Two additional release
attachments provide the complete variant manifest and `SHA256SUMS` for all ZIPs.
Release notes combine the build information with GitHub-generated changes since
the previous latest release.

All matrix builds must succeed before the upload job runs. The upload job rejects
missing or unexpected packages, unreadable ZIPs, unexpected archive members,
wrong embedded application versions, wrong chip targets, and mismatched SDK
versions. It verifies the uploaded attachment names, sizes, and available GitHub
SHA-256 digests before marking the release public.

A failed build leaves its successful artifacts available in the workflow run.
Use **Re-run failed jobs** to continue. A failed upload leaves an unpublished draft;
retrying can replace matching attachments only when the draft and tag refer to
the same source commit. Published releases and tags pointing at other commits
are rejected. Concurrent release runs are serialized.

Successful compilation and package checks do not replace physical hardware
validation. Test the relevant hardware before recommending a release for use.
