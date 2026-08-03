# syntax=docker/dockerfile:1.7

ARG IDF_IMAGE=espressif/idf:release-v6.1
FROM ${IDF_IMAGE}

ARG FIRMWARE_SOURCE_REVISION=unknown
ARG FIRMWARE_SOURCE_URL=https://github.com/78/xiaozhi-esp32

LABEL org.opencontainers.image.title="XiaoZhi firmware builder" \
      org.opencontainers.image.description="Builds one XiaoZhi board configuration with a selected language and wake word" \
      org.opencontainers.image.source="${FIRMWARE_SOURCE_URL}" \
      org.opencontainers.image.revision="${FIRMWARE_SOURCE_REVISION}"

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

WORKDIR /opt/xiaozhi-esp32
COPY . .

RUN chmod 0755 \
      docker/firmware-builder/entrypoint.sh \
      docker/firmware-builder/firmware_builder.py \
    && . "${IDF_PATH}/export.sh" >/dev/null \
    && python3 -m pip install --no-cache-dir oss2==2.19.1 \
    && idf.py --version \
    && python3 scripts/build.py --list-boards --json >/tmp/xiaozhi-boards.json \
    && python3 scripts/build.py --list-languages --json >/tmp/xiaozhi-languages.json

ENV FIRMWARE_SOURCE_DIR=/opt/xiaozhi-esp32 \
    FIRMWARE_OUTPUT_DIR=/output \
    FIRMWARE_SOURCE_REVISION=${FIRMWARE_SOURCE_REVISION} \
    PYTHONUNBUFFERED=1

ENTRYPOINT ["/opt/xiaozhi-esp32/docker/firmware-builder/entrypoint.sh"]
