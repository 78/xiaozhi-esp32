# ESP-IDF 6.0 Migration and Board Compatibility Status

> Last updated: 2026-07-17
> Validated SDK: ESP-IDF v6.0.1
> Scope: 137 board directories and 157 supported build variants defined by `main/boards/**/config.json`.

## Current Status

All 157 IDF 6 variants are build-compatible with ESP-IDF 6.0.1 and passed the latest complete GitHub Actions matrix. Fourteen legacy ESP32-P4 Rev < 3 variants are excluded from the IDF 6 matrix but remain available when building with ESP-IDF < 6. Component versions that use ranges are resolved from the Component Registry at build time; until per-target lock snapshots are committed, this is a source-reproducible build rather than a bit-for-bit dependency-reproducible build.

| Status | Variants | Meaning |
|---|---:|---|
| ✅ Build-compatible | 157 | All 157 full builds completed in GitHub Actions with ESP-IDF 6.0.1; this does not imply hardware or complete peripheral validation |
| 🟡 Feature-degraded subset | 1 | `esp-vocat` builds on IDF 6, but its PCB capacitive slider/button support is disabled pending compatible touch-sensor components |
| 🔴 Build-blocked | 0 | No supported release variant remains blocked at compile or link time |

There are no remaining IDF 6 build blockers. [`78/esp_lcd_nv3023 1.0.1`](https://components.espressif.com/components/78/esp_lcd_nv3023/versions/1.0.1) and [`wvirgil123/sscma_client 1.0.3`](https://components.espressif.com/components/wvirgil123/sscma_client/versions/1.0.3/readme) are consumed directly from the Component Registry, so local copies under the ignored `components/` directory are not required. ESP32-P4 Rev < 3 remains on the ESP-IDF 5.5 legacy build path, while the IDF 6 matrix supports ESP32-P4 v3.x.

Representative local full-build results are shown below. Firmware size and free space are reported by ESP-IDF 6.0.1 `check_sizes.py`. The authoritative per-variant compatibility result is the full GitHub Actions matrix in the next section.

| Chip | Build variant | Application size | Free space in smallest app partition |
|---|---|---:|---:|
| ESP32 | `bread-compact-esp32` | `0x240560` | 23% |
| ESP32-C3 | `esp-hi` | `0x20ef10` | 30% |
| ESP32-C3 | `xmini-c3` | `0x2370f0` | 44% |
| ESP32-C5 | `waveshare-esp32-c5-touch-lcd-1.69` | `0x27cd30` | 37% |
| ESP32-C6 | `waveshare-esp32-c6-lcd-0.85` | `0x280ee0` | 15% |
| ESP32-S3 | `doit-s3-aibox` | `0x21edf0` | 46% |
| ESP32-S3 | `lilygo-t-display-s3-pro-mvsrlora` | `0x2adce0` | 32% |
| ESP32-S3 | `lilygo-t-circle-s3` | `0x2a32f0` | 33% |
| ESP32-S3 | `zhengchen-1.54tft-wifi` | `0x2abdc0` | 32% |
| ESP32-S3 | `kevin-yuying-313lcd` | `0x2c4990` | 30% |
| ESP32-S3 | `otto-robot` | `0x37ff70` | 11% |
| ESP32-S3 | `esp-vocat` | `0x2742f0` | 38% |
| ESP32-S3 | `sensecap-watcher` | `0x2faff0` | 25% |
| ESP32-P4 v3.x | `esp-p4-function-ev-board-p4x` | `0x385420` | 11% |
| ESP32-P4 v3.x | `m5stack-tab5-p4x` | `0x380790` | 11% |

As a negative test, `esp-p4-function-ev-board` (legacy P4 Rev < 3) was rejected as expected by `esp-sr 2.4.6` during IDF 6.0.1 configuration. Legacy P4 variants are therefore version-gated to ESP-IDF < 6 instead of being carried as permanent IDF 6 blockers.

For backward-compatibility regression coverage, `xmini-c3` also completed a full build with ESP-IDF 5.5.4 (application size `0x234920`, 44% free in the smallest app partition). The legacy `esp-p4-function-ev-board` release variant subsequently completed the same 5.5.4 release flow (application size `0x38a130`, 10% free), including merged-binary packaging. This confirms that the compatibility changes for I2S port numbering, LCD I2C configuration, the UHCI DMA dependency, and the pre-v3 P4 selection did not break the existing 5.5 build chain.

## Full-Matrix CI Validation

GitHub Actions run [29534954031](https://github.com/78/xiaozhi-esp32/actions/runs/29534954031) built the complete matrix with the `espressif/idf:v6.0.1` container. The matrix-generation job and all 157 board builds passed.

Results by chip target:

| Chip target | Variants | Passed | Blocked |
|---|---:|---:|---:|
| ESP32 | 7 | 7 | 0 |
| ESP32-C3 | 9 | 9 | 0 |
| ESP32-C5 | 4 | 4 | 0 |
| ESP32-C6 | 9 | 9 | 0 |
| ESP32-S3 | 114 | 114 | 0 |
| ESP32-P4 v3.x | 14 | 14 | 0 |
| **Total** | **157** | **157** | **0** |

`esp-vocat` remains feature-degraded because IDF 6 builds omit the PCB capacitive slider/button path; its CST816 display touch remains enabled.

## ESP32-P4 Silicon Scope and Naming

The IDF 6 release matrix supports only ESP32-P4 v3.x silicon. Each legacy Rev < 3 build keeps its original name and contains `idf_version: "<6.0"`. Variant selection behaves as follows:

| SDK | ESP32-P4 Rev < 3 | ESP32-P4 Rev >= 3 |
|---|---|---|
| ESP-IDF < 6 | Original name without a suffix; adds `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and `CONFIG_ESP32P4_REV_MIN_100=y` | Original name plus `-p4x` |
| ESP-IDF >= 6 | Not listed or built | `-p4x` |

This restores the historical artifact names and keeps the IDF 6 matrix at 157 variants. The IDF 5.5 selection contains 171 variants: 14 P4 `-p4x` artifacts, 14 legacy-P4 artifacts with their original unsuffixed names, and 143 non-P4 variants.

Espressif's current chip-identification table lists v0.0, v1.0, v1.3, v3.0, v3.1, and v3.2, with no v4.0 revision. The public errata history added v3.0/v3.1 information on 2026-02-12 and v3.2 information on 2026-04-20. ESP-IDF 6.0 release notes explicitly describe support for "ESP32-P4 Version3 silicon." See the official [chip revision identification](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32p4/01-chip-identification/index.html), [errata revision history](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32p4/revision-history/index.html), and [ESP-IDF releases](https://github.com/espressif/esp-idf/releases).

## Component Compatibility

| Component/module | Version or approach for IDF 6 | Status | Notes |
|---|---|---|---|
| `78/uart-uhci` | `0.3.2` | ✅ Upstream support | The registry release compiles under IDF 6.0.1 in GitHub Actions |
| `78/uart-eth-modem` | `0.6.0` | ✅ Upstream support | Pinned because it has the required RF-test event API while supporting both ESP-IDF 5.5.2+ and 6.0.1; version 0.6.1 requires IDF 6.0.1+ |
| `espressif/mqtt` | `1.0.0` | ✅ Upstream support | MQTT moved from a built-in SDK component to a Component Manager dependency in IDF 6 |
| `78/esp-ml307` | `3.6.6` + project-level `espressif/mqtt` | ✅ Builds in CI | The upstream source compiles under IDF 6.0.1 when the project supplies the MQTT component moved out of IDF |
| `espressif/esp_hosted` / `esp_wifi_remote` | `2.12.11` / `1.6.2` | ✅ Upstream support | Used for ESP32-P4 Hosted Wi-Fi |
| `espressif/esp_video` | `^2.0.1` | ✅ Upstream support | Currently resolves to 2.3.0 on S3 and 2.0.1 on P4 due to BSP constraints |
| P4 BSP / LCD drivers | BSP `5.2.3`, EK79007/ST7701 `2.0.x` | ✅ Upstream support | Resolves IDF 6 DPI, color-field, and split-driver-component issues |
| `espressif/esp_lcd_st77916` | `2.0.2` | ✅ Upstream support | Major version 2 uses the IDF 6 panel I/O definitions |
| `espressif/esp_lcd_spd2010` | `^2.0.0` (resolved `2.0.0~1`) | ✅ Upstream support | Major version 2 declares ESP-IDF 6 compatibility |
| `espressif/esp_lcd_co5300` | `2.1.0` | ✅ Upstream support | Used with explicit IDF 6-compatible QSPI I/O configuration in board code |
| `esp_emote_expression` | `1.0.2` | ✅ Upstream support | Replaces the previous dependency on the built-in `json` component |
| `wvirgil123/sscma_client` | [`1.0.3`](https://components.espressif.com/components/wvirgil123/sscma_client/versions/1.0.3/readme) | ✅ Upstream support | Uses `espressif/cjson` and the split driver components on IDF 6 while retaining the legacy component names on older IDF releases |
| `espressif/servo` / `espfriends/servo_dog_ctrl` | `1.0.0` / `0.2.0` | ✅ Upstream support | Uses official registry releases; compilation and linking were validated in the full IDF 6.0.1 build of `esp-hi`, with no local override required |
| `78/esp_lcd_nv3023` | [`1.0.1`](https://components.espressif.com/components/78/esp_lcd_nv3023/versions/1.0.1) | ✅ Upstream-based registry release | Mirrors MakerM0 upstream commit `15dae953`; adds IDF 6 color-order and GPIO compatibility while retaining older IDF branches; validated through the registry in a clean `magiclick-c3` full build |
| `llgok/cpp_bus_driver` | Excluded from the IDF 6 baseline | ⚪ Waiting for upstream | Version `2.1.0` still requires local patches for IDF 6. It is used only by the `LILYGO T-Display-P4` source, and that board has no release `config.json`, so the component has been removed until upstream support is available |
| MQTT protocol AES-CTR | PSA Crypto | ✅ Ported | Replaces the legacy AES context API removed by IDF 6 / Mbed TLS 4 |
| BluFi security negotiation (conditional path) | PSA FFDH + SHA-256 + AES-CTR | ✅ Ported | Uses the ESP-IDF 6 security scheme with ffdhe3072 and passed local full ESP32-S3 builds with BluFi enabled on IDF 5.5.4 and 6.0.1. Legacy 1024-bit BluFi clients are not compatible and must be upgraded |
| `espressif/bmi270_sensor` | [`0.1.2`](https://components.espressif.com/components/espressif/bmi270_sensor/versions/0.1.2/readme?language=en) | ✅ Upstream support | Provides IDF 6.0 prebuilt libraries for ESP32-C5 and ESP32-S3; validated by a full `esp-spot-c5` build |
| `touch_slider_sensor` / `touch_button_sensor` | Disabled for IDF 6 | 🟡 Feature gap | Their manifests require IDF < 6.0, so ESP Vocat omits its PCB capacitive slider/button path on IDF 6. CST816 display touch is unaffected |
| ESP32-P4 Rev < 3 / `espressif/esp-sr` | Version-gated to IDF < 6 | 🟡 Legacy SDK path | The 14 legacy variants are restored for IDF 5.5 builds. They remain excluded from IDF 6 because `esp-sr 2.4.6` rejects the old-silicon configuration; ESP32-P4 v3.x is unaffected |

## Per-Board Progress

In the table below, "Board" is the source directory and "Build variant" is the firmware name used by the release script. Different chip revisions of the same board may have different status, so they are split into separate rows when necessary.

| Chip | Board | Build variant | IDF 6.0 status | Current validation | Blocker/next step |
|---|---|---|---|---|---|
| `esp32` | `atommatrix-echo-base` | `atommatrix-echo-base` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32` | `bread-compact-esp32` | `bread-compact-esp32` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32` | `bread-compact-esp32` | `bread-compact-esp32-128x32` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32` | `bread-compact-esp32-lcd` | `bread-compact-esp32-lcd` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32` | `esp32-cgc` | `esp32-cgc` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32` | `esp32-cgc-144` | `esp32-cgc-144` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32` | `waveshare/esp32-touch-lcd-3.5` | `waveshare-esp32-touch-lcd-3.5` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c3` | `esp-hi` | `esp-hi` | ✅ Full build passed | IDF 6.0.1 `idf.py build`; `servo_dog_ctrl 0.2.0` | Hardware smoke, servo, and audio peripheral regression pending |
| `esp32c3` | `kevin-c3` | `kevin-c3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c3` | `lichuang-c3-dev` | `lichuang-c3-dev` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c3` | `magiclick-c3` | `magiclick-c3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c3` | `magiclick-c3-v2` | `magiclick-c3-v2` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c3` | `surfer-c3-1.14tft` | `surfer-c3-1.14tft` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c3` | `xmini-c3` | `xmini-c3` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32c3` | `xmini-c3-4g` | `xmini-c3-4g` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c3` | `xmini-c3-v3` | `xmini-c3-v3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c5` | `esp-sensairshuttle` | `esp-sensairshuttle` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c5` | `esp-spot` | `esp-spot-c5` | ✅ Full build passed | IDF 6.0.1 `idf.py build`; `bmi270_sensor 0.1.2` | Hardware smoke, BMI270, audio, and power-management regression pending |
| `esp32c5` | `movecall-moji2-esp32c5` | `movecall-moji2-esp32c5` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c5` | `waveshare/esp32-c5-touch-lcd-1.69` | `waveshare-esp32-c5-touch-lcd-1.69` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-lcd-0.85` | `waveshare-esp32-c6-lcd-0.85` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-lcd-1.69` | `waveshare-esp32-c6-lcd-1.69` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-touch-amoled-1.32` | `waveshare-esp32-c6-touch-amoled-1.32` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-touch-amoled-1.43` | `waveshare-esp32-c6-touch-amoled-1.43` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-touch-amoled-1.8` | `waveshare-esp32-c6-touch-amoled-1.8` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-touch-amoled-2.06` | `waveshare-esp32-c6-touch-amoled-2.06` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-touch-amoled-2.16` | `waveshare-esp32-c6-touch-amoled-2.16` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-touch-lcd-1.54` | `waveshare-esp32-c6-touch-lcd-1.54` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-touch-lcd-1.83` | `waveshare-esp32-c6-touch-lcd-1.83` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `aipi-lite` | `aipi-lite` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `atk-dnesp32s3` | `atk-dnesp32s3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `atk-dnesp32s3-box` | `atk-dnesp32s3-box` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `atk-dnesp32s3-box0` | `atk-dnesp32s3-box0` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `atk-dnesp32s3-box2-4g` | `atk-dnesp32s3-box2-4g` | ✅ Build and hardware validated | GitHub Actions IDF 6.0.1 full build; maintainer hardware validation | Complete for the IDF 6 migration scope |
| `esp32s3` | `atk-dnesp32s3-box2-wifi` | `atk-dnesp32s3-box2-wifi` | ✅ Build and hardware validated | GitHub Actions IDF 6.0.1 full build; maintainer hardware validation | Complete for the IDF 6 migration scope |
| `esp32s3` | `atk-dnesp32s3-box3` | `atk-dnesp32s3-box3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `atom-echos3r` | `atom-echos3r` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `atoms3-echo-base` | `atoms3-echo-base` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `atoms3r-cam-m12-echo-base` | `atoms3r-cam-m12-echo-base` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `atoms3r-echo-base` | `atoms3r-echo-base` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `atoms3r-echo-pyramid` | `atoms3r-echo-pyramid` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `bread-compact-ml307` | `bread-compact-ml307`<br>`bread-compact-ml307-128x64` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `bread-compact-nt26` | `bread-compact-nt26` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `bread-compact-wifi` | `bread-compact-wifi`<br>`bread-compact-wifi-128x64` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `df-k10` | `df-k10` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `df-s3-ai-cam` | `df-s3-ai-cam` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `doit-s3-aibox` | `doit-s3-aibox` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32s3` | `du-chatx` | `du-chatx` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `electron-bot` | `electron-bot` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `esp-box` | `esp-box` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `esp-box-3` | `esp-box-3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `esp-box-lite` | `esp-box-lite` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `esp-s3-lcd-ev-board` | `esp-s3-lcd-ev-board-1p4`<br>`esp-s3-lcd-ev-board-1p5` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `esp-s3-lcd-ev-board-2` | `esp-s3-lcd-ev-board-2` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `esp-sparkbot` | `esp-sparkbot` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `esp-vocat` | `esp-vocat` | 🟡 Full build passed; feature-degraded | Local ESP-IDF 6.0.1 full build after rebasing onto `origin/main` | PCB capacitive slider/button disabled on IDF 6; CST816 display touch remains enabled; hardware regression pending |
| `esp32s3` | `esp32s3-korvo2-v3` | `esp32s3-korvo2-v3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `esp32s3-korvo2-v3-rndis` | `esp32s3-korvo2-v3-rndis` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `freenove-esp32s3-display-2.8-lcd` | `freenove-esp32s3-display-2.8-lcd` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `genjutech-s3-1.54tft` | `genjutech-s3-1.54tft` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `hu-087` | `hu-087` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `jiuchuan-s3` | `jiuchuan-s3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `kevin-box-2` | `kevin-box-2` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `kevin-sp-v4-dev` | `kevin-sp-v4-dev` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `kevin-yuying-313lcd` | `kevin-yuying-313lcd` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `labplus-ledong-v2` | `labplus-ledong-v2` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `labplus-mpython-v3` | `labplus-mpython-v3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `lceda-course-examples/eda-robot-pro` | `lceda-course-examples-eda-robot-pro` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `lceda-course-examples/eda-super-bear` | `lceda-course-examples-eda-super-bear` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `lceda-course-examples/eda-tv-pro` | `lceda-course-examples-eda-tv-pro` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `lichuang-dev` | `lichuang-dev` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `lilygo-t-cameraplus-s3` | `lilygo-t-cameraplus-s3`<br>`lilygo-t-cameraplus-s3_v1_2` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `lilygo-t-circle-s3` | `lilygo-t-circle-s3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `lilygo-t-display-s3-pro-mvsrlora` | `lilygo-t-display-s3-pro-mvsrlora` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `m5stack-cardputer-adv` | `m5stack-cardputer-adv` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `m5stack-core-s3` | `m5stack-core-s3` | ✅ Build and hardware validated | GitHub Actions IDF 6.0.1 full build; maintainer hardware validation | Complete for the IDF 6 migration scope |
| `esp32s3` | `m5stack-stick-s3` | `m5stack-stick-s3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `m5stack-stopwatch` | `m5stack-stopwatch` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `magiclick-2p4` | `magiclick-2p4` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `magiclick-2p5` | `magiclick-2p5` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `minsi-k08-dual` | `minsi-k08-dual` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `mixgo-nova` | `mixgo-nova` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `movecall-cuican-esp32s3` | `movecall-cuican-esp32s3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `movecall-moji-esp32s3` | `movecall-moji-esp32s3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `nulllab-ai-vox-v3` | `nulllab-ai-vox-v3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `otto-robot` | `otto-robot` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `quandong-s3-dev` | `quandong-s3-dev` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `rymcu/bigsmart` | `rymcu-bigsmart` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `sensecap-watcher` | `sensecap-watcher` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `sp-esp32-s3-1.28-box` | `sp-esp32-s3-1.28-box` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `sp-esp32-s3-1.54-muma` | `sp-esp32-s3-1.54-muma` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `taiji-pi-s3` | `taiji-pi-s3`<br>`taiji-pi-s3-pdm` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `tudouzi` | `tudouzi` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-audio-board` | `waveshare-esp32-s3-audio-board` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-cam` | `waveshare-esp32-s3-cam-2`<br>`waveshare-esp32-s3-cam-2.8`<br>`waveshare-esp32-s3-cam-3.5`<br>`waveshare-esp32-s3-cam-1.83` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-epaper-1.54` | `waveshare-esp32-s3-epaper-1.54-v2`<br>`waveshare-esp32-s3-epaper-1.54-v1` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-epaper-3.97` | `waveshare-esp32-s3-epaper-3.97` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-lcd-0.85` | `waveshare-esp32-s3-lcd-0.85` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-rgb-matrix` | `waveshare-esp32-s3-rgb-matrix` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-rlcd-4.2` | `waveshare-esp32-s3-rlcd-4.2` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-1.32` | `waveshare-esp32-s3-touch-amoled-1.32` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-1.43c` | `waveshare-esp32-s3-touch-amoled-1.43c` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-1.75` | `waveshare-esp32-s3-touch-amoled-1.75`<br>`waveshare-esp32-s3-touch-amoled-1.75c` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-1.8` | `waveshare-esp32-s3-touch-amoled-1.8` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-1.8-v2` | `waveshare-esp32-s3-touch-amoled-1.8-v2` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-2.06` | `waveshare-esp32-s3-touch-amoled-2.06` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-2.16` | `waveshare-esp32-s3-touch-amoled-2.16` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.46` | `waveshare-esp32-s3-touch-lcd-1.46` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.54` | `waveshare-esp32-s3-touch-lcd-1.54` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.83` | `waveshare-esp32-s3-touch-lcd-1.83` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.85` | `waveshare-esp32-s3-touch-lcd-1.85` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.85b` | `waveshare-esp32-s3-touch-lcd-1.85b` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.85c` | `waveshare-esp32-s3-touch-lcd-1.85c` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-3.49` | `waveshare-esp32-s3-touch-lcd-3.49` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-3.5` | `waveshare-esp32-s3-touch-lcd-3.5` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-3.5b` | `waveshare-esp32-s3-touch-lcd-3.5b` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-4.3c` | `waveshare-esp32-s3-touch-lcd-4.3c` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-4b` | `waveshare-esp32-s3-touch-lcd-4b` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-7c` | `waveshare-esp32-s3-touch-lcd-7c` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `xingzhi-abs-2.0` | `xingzhi-abs-2.0` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `xingzhi-cube-0.85tft-ml307` | `xingzhi-cube-0.85tft-ml307` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `xingzhi-cube-0.85tft-wifi` | `xingzhi-cube-0.85tft-wifi` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `xingzhi-cube-0.96oled-ml307` | `xingzhi-cube-0.96oled-ml307` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `xingzhi-cube-0.96oled-wifi` | `xingzhi-cube-0.96oled-wifi` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `xingzhi-cube-1.54tft-ml307` | `xingzhi-cube-1.54tft-ml307`<br>`xingzhi-cube-1.54tft-ml307-wechatui` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `xingzhi-cube-1.54tft-wifi` | `xingzhi-cube-1.54tft-wifi` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `xingzhi-metal-1.54-wifi` | `xingzhi-metal-1.54-wifi` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `yunliao-s3` | `yunliao-s3` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `zhengchen-1.54tft-ml307` | `zhengchen-1.54tft-ml307` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `zhengchen-1.54tft-wifi` | `zhengchen-1.54tft-wifi` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `zhengchen-cam` | `zhengchen-cam` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32s3` | `zhengchen-cam-ml307` | `zhengchen-cam-ml307` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32p4` | `esp-p4-function-ev-board` | `esp-p4-function-ev-board-p4x` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32p4` | `m5stack-tab5` | `m5stack-tab5-p4x` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32p4` | `waveshare/esp32-p4-nano` | `waveshare-esp32-p4-nano-10.1-a-p4x` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32p4` | `waveshare/esp32-p4-wifi6-touch-lcd` | `waveshare-esp32-p4-wifi6-touch-lcd-4b-p4x`<br>`waveshare-esp32-p4-wifi6-touch-lcd-4.3-p4x`<br>`waveshare-esp32-p4-wifi6-touch-lcd-5-p4x`<br>`waveshare-esp32-p4-wifi6-touch-lcd-7b-p4x`<br>`waveshare-esp32-p4-wifi6-touch-lcd-3.4c-p4x`<br>`waveshare-esp32-p4-wifi6-touch-lcd-4c-p4x`<br>`waveshare-esp32-p4-wifi6-touch-lcd-7-p4x`<br>`waveshare-esp32-p4-wifi6-touch-lcd-8-p4x`<br>`waveshare-esp32-p4-wifi6-touch-lcd-10.1-p4x` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32p4` | `waveshare/esp32-p4-wifi6-touch-lcd-3.5` | `waveshare-esp32-p4-wifi6-touch-lcd-3.5-p4x` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |
| `esp32p4` | `wireless-tag-wtp4c5mp07s` | `wireless-tag-wtp4c5mp07s-p4x` | ✅ Full build passed | GitHub Actions IDF 6.0.1 full build | Hardware smoke/peripheral regression pending |

## Next Steps and Acceptance Criteria

1. Run the new IDF 5.5 compatibility and IDF 6 BluFi CI jobs, then hardware-test at least one P4 v1.3 device on the legacy SDK path. When IDF 6-compatible releases of `touch_slider_sensor` and `touch_button_sensor` become available, re-enable and hardware-test the ESP Vocat PCB capacitive slider/button path.
2. For every green variant, complete a minimal hardware smoke test covering boot, networking, audio input/output, display/touch when present, camera when present, and 4G/Ethernet when present.
3. Perform a physical negative test for the `xmini-c3`/`xmini-c3-v3` firmware guard. CI proves that both images compile; it does not prove that a wrong image is safely rejected. Acceptance requires flashing each wrong image to a sacrificial or recoverable board and verifying that startup stops before any board-specific power or peripheral initialization can cause damage.
4. Keep third-party experiments under the ignored `components/` directory out of the migration branch. The reproducible baseline must use the published `78/esp_lcd_nv3023 1.0.1` and `wvirgil123/sscma_client 1.0.3` packages. `espfriends/servo_dog_ctrl 0.2.0` is an upstream registry dependency and needs no local override.

## Reproduction

```bash
source ~/.espressif/v6.0.1/esp-idf/export.sh
python scripts/release.py <board> --name <variant>
```

This document is a migration-status snapshot. Build compatibility and hardware compatibility are tracked separately. Add "hardware validated" to a board only after validation on physical hardware is complete.
