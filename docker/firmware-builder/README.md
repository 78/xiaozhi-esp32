# Firmware builder container

This image builds one board configuration. The caller selects the board
directory, board name, UI language, and ESP-SR wake-word model. The builder
derives the OTA-reported board type from the selected board's `config.json`.

## Build the image

```bash
docker build \
  --platform linux/arm64 \
  --build-arg FIRMWARE_SOURCE_REVISION="$(git rev-parse HEAD)" \
  -f docker/firmware-builder/Dockerfile \
  -t xiaozhi/firmware-builder:idf61-arm64 .
```

The base image defaults to `espressif/idf:release-v6.1`.

`scripts/build.py` configures the target, generated sdkconfig defaults, and
board name in one `idf.py reconfigure` call. Component Manager resolves and
populates `managed_components` during that step, so each fresh ECI source clone
must have outbound network access.

## Run one build

```bash
docker run --rm --platform linux/arm64 \
  -e FIRMWARE_BOARD_DIR=xmini/c3 \
  -e FIRMWARE_BOARD_NAME=xmini-c3 \
  -e FIRMWARE_LANGUAGE=zh-CN \
  -e FIRMWARE_WAKE_WORD=nihaoxiaozhi \
  -v "$PWD/output:/output" \
  xiaozhi/firmware-builder:idf61-arm64
```

The board fields intentionally follow `main/boards/**/config.json`:

- `board_dir` is the path relative to `main/boards` and is passed as the
  positional argument to `scripts/build.py`;
- `board_type` is derived from the top-level `type` reported by the firmware
  to OTA; callers do not supply it;
- `board_name` is the selected `builds[].name` and is passed to
  `scripts/build.py --name`.

The builder validates `board_dir` and `board_name` against the checked-out
source and records the derived `board_type` before starting a build.

Each successful job writes:

- `xiaozhi.bin`: application/OTA image;
- `merged-binary.bin`: full flash image;
- `build.log`: complete compiler output;
- `manifest.json`: inputs, tool versions, source revision, sizes, and SHA-256
  checksums.

To upload the job output to OSS, also pass:

```text
FIRMWARE_OSS_UPLOAD=true
FIRMWARE_OSS_ENDPOINT=oss-cn-shenzhen.aliyuncs.com
FIRMWARE_OSS_PREFIX=custom_firmwares
OSS_BUCKET_NAME=<bucket>
OSS_ACCESS_KEY_ID=<access-key-id>
OSS_ACCESS_KEY_SECRET=<access-key-secret>
FIRMWARE_JOB_ID=<unique-safe-job-id>
```

The builder uploads the two firmware images, `build.log`, and `manifest.json`
to `custom_firmwares/<job-id>/`. The manifest is uploaded last so consumers do
not observe a completed job before its other objects are available.

Use a unique empty output directory for each job. In ECI, pass the same inputs
as container environment variables and upload the output directory to OSS after
the process exits.

ESP-IDF uses Ninja, which automatically builds in parallel using the CPUs
visible to the container. Allocate at least 8 vCPUs to an ECI build job when
build latency is more important than compute cost; forcing a fixed `-j` value is
unnecessary and can oversubscribe smaller instances.

The production image targets `linux/arm64`. Create the ECI container group with
`CpuArchitecture=ARM64`, `Cpu=8`, and a memory size selected for the requested
board. The image architecture and ECI architecture must match.
