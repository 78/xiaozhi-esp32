# ESP-IDF 6.0 Migration and Board Compatibility Status

> Last updated: 2026-07-16
> Validated SDK: ESP-IDF v6.0.1
> Scope: 137 board directories and 157 supported build variants defined by `main/boards/**/config.json`.

## Current Status

The shared code has been migrated to IDF 6, and representative ESP32, C3, C5, C6, S3, and ESP32-P4 v3.x variants have been fully built locally with compatibility copies of third-party components. Of the 157 supported variants, 7 have passed that local full-build baseline, 148 have completed the shared migration but still require reproducible per-board builds, and 2 have board-specific upstream blockers. Fourteen legacy ESP32-P4 Rev < 3 variants have been removed from the supported release matrix.

| Status | Variants | Meaning |
|---|---:|---|
| ✅ Full build passed | 7 | Clean/full build completed locally with ESP-IDF 6.0.1; this does not imply hardware or complete peripheral validation |
| 🟡 Ported, per-board build pending | 148 | Shared dependencies and source code have been migrated, but this variant has not yet received an individual full build |
| 🔴 Blocked by upstream | 2 | An upstream component either has no IDF 6 artifact or explicitly rejects IDF 6 |

By chip family, the board-specific blockers affect one ESP32-C5 variant and one ESP32-S3 variant. In addition, the upstream-only GitHub Actions baseline is currently stopped by two shared manifest dependencies: `78/esp_lcd_nv3023 1.0.0` on ESP32, C3, C6, and P4, and `wvirgil123/sscma_client 1.0.2` on S3 and C5. These shared failures do not prove that every affected board is incompatible; both dependencies are resolved before the build reaches board-specific source code. Legacy ESP32-P4 Rev < 3 is no longer supported, while ESP32-P4 v3.x remains in scope.

Full-build results for representative variants are shown below. Firmware size and free space are reported by ESP-IDF 6.0.1 `check_sizes.py`. `xmini-c3` was rebuilt successfully against the final source state, and `esp-p4-function-ev-board` was rebuilt successfully after removing `cpp_bus_driver`.

| Chip | Build variant | Application size | Free space in smallest app partition |
|---|---|---:|---:|
| ESP32 | `bread-compact-esp32` | `0x240560` | 23% |
| ESP32-C3 | `esp-hi` | `0x20ef10` | 30% |
| ESP32-C3 | `xmini-c3` | `0x2370f0` | 44% |
| ESP32-C5 | `waveshare-esp32-c5-touch-lcd-1.69` | `0x27cd30` | 37% |
| ESP32-C6 | `waveshare-esp32-c6-lcd-0.85` | `0x280ee0` | 15% |
| ESP32-S3 | `doit-s3-aibox` | `0x21edf0` | 46% |
| ESP32-P4 v3.x | `esp-p4-function-ev-board` | `0x385420` | 11% |

As a negative test, `esp-p4-function-ev-board` (legacy P4 Rev < 3) was rejected as expected by `esp-sr 2.4.6` during IDF 6.0.1 configuration. Legacy P4 variants have therefore been removed from the supported release matrix instead of being carried as permanent blockers.

For backward-compatibility regression coverage, `xmini-c3` also completed a full build with ESP-IDF 5.5.4 (application size `0x234920`, 44% free in the smallest app partition). This confirms that the compatibility changes for I2S port numbering, LCD I2C configuration, and the UHCI DMA dependency did not break the existing 5.5 build chain.

## Upstream-Only CI Baseline

GitHub Actions run [29432645739](https://github.com/78/xiaozhi-esp32/actions/runs/29432645739) built the complete 157-variant matrix with the `espressif/idf:v6.0.1` container and only committed files plus published registry components. The matrix-generation job passed, while all 157 board build jobs were stopped by a shared upstream dependency before a reproducible board firmware could be produced.

Representative logs and the target-conditional dependency rules identify the first blocker for each chip family:

| Chip target | Variants in matrix | First shared blocker | Failure stage |
|---|---:|---|---|
| ESP32 | 7 | `78/esp_lcd_nv3023 1.0.0` | Compile: removed LCD color-order API |
| ESP32-C3 | 9 | `78/esp_lcd_nv3023 1.0.0` | Compile: removed LCD color-order API |
| ESP32-C5 | 4 | `wvirgil123/sscma_client 1.0.2` | Configure: removed built-in `json` component |
| ESP32-C6 | 9 | `78/esp_lcd_nv3023 1.0.0` | Compile: removed LCD color-order API |
| ESP32-S3 | 114 | `wvirgil123/sscma_client 1.0.2` | Configure: removed built-in `json` component |
| ESP32-P4 v3.x | 14 | `78/esp_lcd_nv3023 1.0.0` | Compile: removed LCD color-order API |

Therefore, 39 jobs are currently masked by `esp_lcd_nv3023` and 118 jobs by `sscma_client`. This is a dependency-resolution result, not evidence of 157 independent board-porting defects. Once upstream-compatible releases are published, the entire matrix must be rerun to expose any later board-specific failures.

## ESP32-P4 Silicon Scope and Naming

The release matrix supports only ESP32-P4 v3.x silicon. The former `-p4x` build-name suffix has been removed because it no longer distinguishes two supported product lines; all 14 legacy Rev < 3 build variants are out of scope.

Espressif's current chip-identification table lists v0.0, v1.0, v1.3, v3.0, v3.1, and v3.2, with no v4.0 revision. The public errata history added v3.0/v3.1 information on 2026-02-12 and v3.2 information on 2026-04-20. ESP-IDF 6.0 release notes explicitly describe support for "ESP32-P4 Version3 silicon." See the official [chip revision identification](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32p4/01-chip-identification/index.html), [errata revision history](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32p4/revision-history/index.html), and [ESP-IDF releases](https://github.com/espressif/esp-idf/releases).

## Component Compatibility

| Component/module | Version or approach for IDF 6 | Status | Notes |
|---|---|---|---|
| `78/uart-uhci` | `0.3.0` | ✅ Upstream support | The registry release compiles under IDF 6.0.1 in GitHub Actions |
| `78/uart-eth-modem` | `0.6.0` | ✅ Upstream support | Updated for the new event callback and depends on `uart-uhci 0.3.x` |
| `espressif/mqtt` | `1.0.0` | ✅ Upstream support | MQTT moved from a built-in SDK component to a Component Manager dependency in IDF 6 |
| `78/esp-ml307` | `3.6.5` + project-level `espressif/mqtt` | ✅ Builds in CI | The upstream source compiles under IDF 6.0.1 when the project supplies the MQTT component moved out of IDF |
| `espressif/esp_hosted` / `esp_wifi_remote` | `2.12.11` / `1.6.2` | ✅ Upstream support | Used for ESP32-P4 Hosted Wi-Fi |
| `espressif/esp_video` | `^2.0.1` | ✅ Upstream support | Currently resolves to 2.3.0 on S3 and 2.0.1 on P4 due to BSP constraints |
| P4 BSP / LCD drivers | BSP `5.2.3`, EK79007/ST7701 `2.0.x` | ✅ Upstream support | Resolves IDF 6 DPI, color-field, and split-driver-component issues |
| `esp_emote_expression` | `1.0.2` | ✅ Upstream support | Replaces the previous dependency on the built-in `json` component |
| `wvirgil123/sscma_client` | `1.0.2` | 🔴 Waiting for upstream | Its CMake metadata still requires the built-in `json` component removed by IDF 6; this dependency currently stops all S3/C5 CI jobs during configuration |
| `espressif/servo` / `espfriends/servo_dog_ctrl` | `1.0.0` / `0.2.0` | ✅ Upstream support | Uses official registry releases; compilation and linking were validated in the full IDF 6.0.1 build of `esp-hi`, with no local override required |
| `78/esp_lcd_nv3023` | `1.0.0` | 🔴 Waiting for upstream | Uses the removed `rgb_endian` field and old `LCD_RGB_ENDIAN_*` constants; because the project manifest currently declares it globally, it stops ESP32/C3/C6/P4 CI jobs even when the board does not use this panel |
| `llgok/cpp_bus_driver` | Excluded from the IDF 6 baseline | ⚪ Waiting for upstream | Version `2.1.0` still requires local patches for IDF 6. It is used only by the `LILYGO T-Display-P4` source, and that board has no release `config.json`, so the component has been removed until upstream support is available |
| MQTT protocol AES-CTR | PSA Crypto | ✅ Ported | Replaces the legacy AES context API removed by IDF 6 / Mbed TLS 4 |
| BluFi security negotiation (conditional path) | Legacy Mbed TLS DHM/AES API | ⚪ Not yet migrated | None of the 157 supported release variants enables it. IDF 6 removed `mbedtls/dhm.h`; compatibility of the official PSA FFDH + AES-CTR approach with the existing mobile-client protocol must be evaluated |
| `espressif/bmi270_sensor` | Latest version remains `0.1.1` | 🔴 Unsupported | Provides prebuilt libraries only for IDF 5.3/5.4/5.5; affects ESP Spot C5 and ESP Vocat |
| `touch_slider_sensor` / `touch_button_sensor` | Current versions | 🔴 Unsupported | Their manifests require IDF < 6.0; affects ESP Vocat |
| ESP32-P4 Rev < 3 / `espressif/esp-sr` | Not included | ⚫ Out of scope | Legacy P4 support and its 14 release variants have been removed; ESP32-P4 v3.x is unaffected |

## Per-Board Progress

In the table below, "Board" is the source directory and "Build variant" is the firmware name used by the release script. Different chip revisions of the same board may have different status, so they are split into separate rows when necessary.

| Chip | Board | Build variant | IDF 6.0 status | Current validation | Blocker/next step |
|---|---|---|---|---|---|
| `esp32` | `atommatrix-echo-base` | `atommatrix-echo-base` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32` | `bread-compact-esp32` | `bread-compact-esp32` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32` | `bread-compact-esp32` | `bread-compact-esp32-128x32` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32` | `bread-compact-esp32-lcd` | `bread-compact-esp32-lcd` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32` | `esp32-cgc` | `esp32-cgc` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32` | `esp32-cgc-144` | `esp32-cgc-144` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32` | `waveshare/esp32-touch-lcd-3.5` | `waveshare-esp32-touch-lcd-3.5` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c3` | `esp-hi` | `esp-hi` | ✅ Full build passed | IDF 6.0.1 `idf.py build`; `servo_dog_ctrl 0.2.0` | Hardware smoke, servo, and audio peripheral regression pending |
| `esp32c3` | `kevin-c3` | `kevin-c3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c3` | `lichuang-c3-dev` | `lichuang-c3-dev` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c3` | `magiclick-c3` | `magiclick-c3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c3` | `magiclick-c3-v2` | `magiclick-c3-v2` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c3` | `surfer-c3-1.14tft` | `surfer-c3-1.14tft` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c3` | `xmini-c3` | `xmini-c3` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32c3` | `xmini-c3-4g` | `xmini-c3-4g` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c3` | `xmini-c3-v3` | `xmini-c3-v3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c5` | `esp-sensairshuttle` | `esp-sensairshuttle` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c5` | `esp-spot` | `esp-spot-c5` | 🔴 Blocked by upstream | Component artifact review + configure-time guard | BMI270 0.1.1 has no prebuilt ESP-IDF 6.0 library |
| `esp32c5` | `movecall-moji2-esp32c5` | `movecall-moji2-esp32c5` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c5` | `waveshare/esp32-c5-touch-lcd-1.69` | `waveshare-esp32-c5-touch-lcd-1.69` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-lcd-0.85` | `waveshare-esp32-c6-lcd-0.85` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32c6` | `waveshare/esp32-c6-lcd-1.69` | `waveshare-esp32-c6-lcd-1.69` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c6` | `waveshare/esp32-c6-touch-amoled-1.32` | `waveshare-esp32-c6-touch-amoled-1.32` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c6` | `waveshare/esp32-c6-touch-amoled-1.43` | `waveshare-esp32-c6-touch-amoled-1.43` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c6` | `waveshare/esp32-c6-touch-amoled-1.8` | `waveshare-esp32-c6-touch-amoled-1.8` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c6` | `waveshare/esp32-c6-touch-amoled-2.06` | `waveshare-esp32-c6-touch-amoled-2.06` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c6` | `waveshare/esp32-c6-touch-amoled-2.16` | `waveshare-esp32-c6-touch-amoled-2.16` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c6` | `waveshare/esp32-c6-touch-lcd-1.54` | `waveshare-esp32-c6-touch-lcd-1.54` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32c6` | `waveshare/esp32-c6-touch-lcd-1.83` | `waveshare-esp32-c6-touch-lcd-1.83` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `aipi-lite` | `aipi-lite` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atk-dnesp32s3` | `atk-dnesp32s3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atk-dnesp32s3-box` | `atk-dnesp32s3-box` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atk-dnesp32s3-box0` | `atk-dnesp32s3-box0` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atk-dnesp32s3-box2-4g` | `atk-dnesp32s3-box2-4g` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atk-dnesp32s3-box2-wifi` | `atk-dnesp32s3-box2-wifi` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atk-dnesp32s3-box3` | `atk-dnesp32s3-box3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atom-echos3r` | `atom-echos3r` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atoms3-echo-base` | `atoms3-echo-base` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atoms3r-cam-m12-echo-base` | `atoms3r-cam-m12-echo-base` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atoms3r-echo-base` | `atoms3r-echo-base` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `atoms3r-echo-pyramid` | `atoms3r-echo-pyramid` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `bread-compact-ml307` | `bread-compact-ml307`<br>`bread-compact-ml307-128x64` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `bread-compact-nt26` | `bread-compact-nt26` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `bread-compact-wifi` | `bread-compact-wifi`<br>`bread-compact-wifi-128x64` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `df-k10` | `df-k10` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `df-s3-ai-cam` | `df-s3-ai-cam` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `doit-s3-aibox` | `doit-s3-aibox` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32s3` | `du-chatx` | `du-chatx` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `electron-bot` | `electron-bot` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `esp-box` | `esp-box` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `esp-box-3` | `esp-box-3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `esp-box-lite` | `esp-box-lite` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `esp-s3-lcd-ev-board` | `esp-s3-lcd-ev-board-1p4`<br>`esp-s3-lcd-ev-board-1p5` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `esp-s3-lcd-ev-board-2` | `esp-s3-lcd-ev-board-2` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `esp-sparkbot` | `esp-sparkbot` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `esp-vocat` | `esp-vocat` | 🔴 Blocked by upstream | Component artifact review + configure-time guard | BMI270 0.1.1 has no IDF 6 library; touch_slider/touch_button require IDF < 6.0 |
| `esp32s3` | `esp32s3-korvo2-v3` | `esp32s3-korvo2-v3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `esp32s3-korvo2-v3-rndis` | `esp32s3-korvo2-v3-rndis` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `freenove-esp32s3-display-2.8-lcd` | `freenove-esp32s3-display-2.8-lcd` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `genjutech-s3-1.54tft` | `genjutech-s3-1.54tft` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `hu-087` | `hu-087` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `jiuchuan-s3` | `jiuchuan-s3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `kevin-box-2` | `kevin-box-2` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `kevin-sp-v4-dev` | `kevin-sp-v4-dev` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `kevin-yuying-313lcd` | `kevin-yuying-313lcd` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `labplus-ledong-v2` | `labplus-ledong-v2` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `labplus-mpython-v3` | `labplus-mpython-v3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `lceda-course-examples/eda-robot-pro` | `lceda-course-examples-eda-robot-pro` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `lceda-course-examples/eda-super-bear` | `lceda-course-examples-eda-super-bear` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `lceda-course-examples/eda-tv-pro` | `lceda-course-examples-eda-tv-pro` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `lichuang-dev` | `lichuang-dev` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `lilygo-t-cameraplus-s3` | `lilygo-t-cameraplus-s3`<br>`lilygo-t-cameraplus-s3_v1_2` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `lilygo-t-circle-s3` | `lilygo-t-circle-s3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `lilygo-t-display-s3-pro-mvsrlora` | `lilygo-t-display-s3-pro-mvsrlora` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `m5stack-cardputer-adv` | `m5stack-cardputer-adv` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `m5stack-core-s3` | `m5stack-core-s3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `m5stack-stick-s3` | `m5stack-stick-s3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `m5stack-stopwatch` | `m5stack-stopwatch` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `magiclick-2p4` | `magiclick-2p4` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `magiclick-2p5` | `magiclick-2p5` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `minsi-k08-dual` | `minsi-k08-dual` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `mixgo-nova` | `mixgo-nova` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `movecall-cuican-esp32s3` | `movecall-cuican-esp32s3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `movecall-moji-esp32s3` | `movecall-moji-esp32s3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `nulllab-ai-vox-v3` | `nulllab-ai-vox-v3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `otto-robot` | `otto-robot` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `quandong-s3-dev` | `quandong-s3-dev` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `rymcu/bigsmart` | `rymcu-bigsmart` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `sensecap-watcher` | `sensecap-watcher` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `sp-esp32-s3-1.28-box` | `sp-esp32-s3-1.28-box` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `sp-esp32-s3-1.54-muma` | `sp-esp32-s3-1.54-muma` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `taiji-pi-s3` | `taiji-pi-s3`<br>`taiji-pi-s3-pdm` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `tudouzi` | `tudouzi` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-audio-board` | `waveshare-esp32-s3-audio-board` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-cam` | `waveshare-esp32-s3-cam-2`<br>`waveshare-esp32-s3-cam-2.8`<br>`waveshare-esp32-s3-cam-3.5`<br>`waveshare-esp32-s3-cam-1.83` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-epaper-1.54` | `waveshare-esp32-s3-epaper-1.54-v2`<br>`waveshare-esp32-s3-epaper-1.54-v1` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-epaper-3.97` | `waveshare-esp32-s3-epaper-3.97` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-lcd-0.85` | `waveshare-esp32-s3-lcd-0.85` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-rgb-matrix` | `waveshare-esp32-s3-rgb-matrix` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-rlcd-4.2` | `waveshare-esp32-s3-rlcd-4.2` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-1.32` | `waveshare-esp32-s3-touch-amoled-1.32` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-1.43c` | `waveshare-esp32-s3-touch-amoled-1.43c` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-1.75` | `waveshare-esp32-s3-touch-amoled-1.75`<br>`waveshare-esp32-s3-touch-amoled-1.75c` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-1.8` | `waveshare-esp32-s3-touch-amoled-1.8` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-1.8-v2` | `waveshare-esp32-s3-touch-amoled-1.8-v2` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-2.06` | `waveshare-esp32-s3-touch-amoled-2.06` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-amoled-2.16` | `waveshare-esp32-s3-touch-amoled-2.16` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.46` | `waveshare-esp32-s3-touch-lcd-1.46` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.54` | `waveshare-esp32-s3-touch-lcd-1.54` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.83` | `waveshare-esp32-s3-touch-lcd-1.83` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.85` | `waveshare-esp32-s3-touch-lcd-1.85` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.85b` | `waveshare-esp32-s3-touch-lcd-1.85b` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-1.85c` | `waveshare-esp32-s3-touch-lcd-1.85c` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-3.49` | `waveshare-esp32-s3-touch-lcd-3.49` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-3.5` | `waveshare-esp32-s3-touch-lcd-3.5` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-3.5b` | `waveshare-esp32-s3-touch-lcd-3.5b` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-4.3c` | `waveshare-esp32-s3-touch-lcd-4.3c` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-4b` | `waveshare-esp32-s3-touch-lcd-4b` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `waveshare/esp32-s3-touch-lcd-7c` | `waveshare-esp32-s3-touch-lcd-7c` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `xingzhi-abs-2.0` | `xingzhi-abs-2.0` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `xingzhi-cube-0.85tft-ml307` | `xingzhi-cube-0.85tft-ml307` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `xingzhi-cube-0.85tft-wifi` | `xingzhi-cube-0.85tft-wifi` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `xingzhi-cube-0.96oled-ml307` | `xingzhi-cube-0.96oled-ml307` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `xingzhi-cube-0.96oled-wifi` | `xingzhi-cube-0.96oled-wifi` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `xingzhi-cube-1.54tft-ml307` | `xingzhi-cube-1.54tft-ml307`<br>`xingzhi-cube-1.54tft-ml307-wechatui` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `xingzhi-cube-1.54tft-wifi` | `xingzhi-cube-1.54tft-wifi` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `xingzhi-metal-1.54-wifi` | `xingzhi-metal-1.54-wifi` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `yunliao-s3` | `yunliao-s3` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `zhengchen-1.54tft-ml307` | `zhengchen-1.54tft-ml307` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `zhengchen-1.54tft-wifi` | `zhengchen-1.54tft-wifi` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `zhengchen-cam` | `zhengchen-cam` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32s3` | `zhengchen-cam-ml307` | `zhengchen-cam-ml307` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32p4` | `esp-p4-function-ev-board` | `esp-p4-function-ev-board` | ✅ Full build passed | IDF 6.0.1 `idf.py build` | Hardware smoke/peripheral regression pending |
| `esp32p4` | `m5stack-tab5` | `m5stack-tab5` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32p4` | `waveshare/esp32-p4-nano` | `waveshare-esp32-p4-nano-10.1-a` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32p4` | `waveshare/esp32-p4-wifi6-touch-lcd` | `waveshare-esp32-p4-wifi6-touch-lcd-4b`<br>`waveshare-esp32-p4-wifi6-touch-lcd-4.3`<br>`waveshare-esp32-p4-wifi6-touch-lcd-5`<br>`waveshare-esp32-p4-wifi6-touch-lcd-7b`<br>`waveshare-esp32-p4-wifi6-touch-lcd-3.4c`<br>`waveshare-esp32-p4-wifi6-touch-lcd-4c`<br>`waveshare-esp32-p4-wifi6-touch-lcd-7`<br>`waveshare-esp32-p4-wifi6-touch-lcd-8`<br>`waveshare-esp32-p4-wifi6-touch-lcd-10.1` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32p4` | `waveshare/esp32-p4-wifi6-touch-lcd-3.5` | `waveshare-esp32-p4-wifi6-touch-lcd-3.5` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |
| `esp32p4` | `wireless-tag-wtp4c5mp07s` | `wireless-tag-wtp4c5mp07s` | 🟡 Ported, per-board build pending | Dependency and shared-source static review | Run per-board CI and hardware peripheral regression |

## Next Steps and Acceptance Criteria

1. After the two shared CI blockers are cleared, reassess the two known board-specific blocker categories: BMI270 and the ESP Vocat touch components. Legacy P4 Rev < 3 is out of scope.
2. Wait for IDF 6-compatible upstream releases of `esp_lcd_nv3023` and `sscma_client`, then rerun the 157-variant GitHub Actions matrix. Do not commit the ignored third-party copies under `components/`. `servo_dog_ctrl` already uses upstream version `0.2.0` and is no longer a local override.
3. For every green variant, complete a minimal hardware smoke test covering boot, networking, audio input/output, display/touch when present, camera when present, and 4G/Ethernet when present.
4. Keep third-party compatibility experiments under the ignored `components/` directory out of the migration branch. A board may be marked reproducibly green only when it builds from the committed project plus published registry components.

## Reproduction

```bash
source ~/.espressif/v6.0.1/esp-idf/export.sh
python scripts/release.py <board> --name <variant>
```

This document is a migration-status snapshot. Change a variant from yellow to green only after it completes a full ESP-IDF 6.0.1 build. Add "hardware validated" to the notes only after validation on physical hardware is complete.
