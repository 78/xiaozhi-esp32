# AudioCodec 输入通道审计

审计日期：2026-09-29。修改前源码基线：`cf2024f`。逐板清单保留修改前的审计结果；本轮调整见下节。

## 本轮调整

按单麦默认策略修改以下路径，保留 AFE 对多麦输入的支持：

| 板子 | 修改前 | 修改后默认 | 开发者实验接口 |
|---|---|---|---|
| Box Lite | MMR / MM | MR / M | `BoxAudioCodecLite` 新增末尾参数 `microphone_channels=1`；显式传 `2` 测试 MMR / MM，参考开关保持原有逻辑。 |
| ESP32-S31-Korvo-1 | MM | M | 板级 `AUDIO_INPUT_CHANNELS` 默认为 `1`，改为 `2` 仍可测试 MM。没有参考，不应声明 MMR。 |
| T-CameraPlus-S3 V1.0/V1.1 | 单声道采集，却声明 MR | M | 关闭错误的参考声明，与 RX 数据一致。V1.2 原本就是 M。 |

其余 M / MR 路径保留。两款参考来源待核实的 LILYGO 板子仍需硬件确认。改动不能代替实机验证，也不表示已确认 Box Lite 的故障根因。

开发接口与验证要求见 [音频设计](../main/audio/README.md#board-input-layouts)。

## 修改前审计：范围与结论

检查了 `main/audio/`、全部板级 codec、`GetAudioCodec()` 构造参数、自定义派生类，以及所有 `input_channels_` / `input_reference_` 赋值。覆盖 150 个带 `config.json` 的板级目录，另包括 `kevin/sp-v3-dev` 和 `lilygo/t-display-p4` 两个源码目录，共 152 个目录。一个目录可能有多个配置或硬件版本；本表不把目录数当成发布固件数量。

格式描述的是 codec 向音频引擎声明的数据语义；小资源芯片可能使用 LiteAudioEngine，并不实际创建 AFE。I2S slot 数、ADC 物理输入数与上层 mic 数不能混为一谈。`MR` 声明也不证明参考输入的接线、波形与时序经过实机验证。

- `M`：一路麦克风，没有参考。可以正常唤醒和对话；不提供设备端 AEC 所需的参考。
- `MR`：一路麦克风和一路播放参考。
- `MM`：两路麦克风，没有参考。
- `MMR`：两路麦克风和一路播放参考。

**不能把全部板子改为 MR。适合采用的单麦策略是：有有效参考时 MR，无参考时 M。** 已有绝大多数实现遵循这个通道数量策略，只有 Box Lite 与 ESP32-S31-Korvo-1 明确配置了双麦输入。

## 修改前审计：双麦配置

| 板子 | 实现 | 当前格式 | 若决定统一单麦 |
|---|---|---|---|
| ESP32-S3-Box-Lite | BoxAudioCodecLite | 音频处理开启时 MMR，否则 MM | `input_channels_ = input_reference_ ? 2 : 1`；当前 EnableInput/Read 随之调整为单麦采集与排列。需实机验证单通道采集、软件参考和通话。 |
| ESP32-S31-Korvo-1 | Es8389AudioCodec | MM | 当前 `AUDIO_INPUT_CHANNELS=2` 且 `input_reference_=false`；单麦策略应为 M，不能把第二路麦克风标成 R。只改该板输入配置，不宜修改 ES8389 公共类或输出立体声配置。 |

代码依据：[Box Lite](../main/boards/espressif/esp32-s3-box-lite/box_audio_codec_lite.cc)、[S31 Korvo 配置](../main/boards/espressif/esp32-s31-korvo-1/config.h)、[ES8389](../main/audio/codecs/es8389_audio_codec.cc)。当前正点原子 Box2 WiFi / 4G 均使用 ES8389 构造函数默认的单麦输入，并非双麦。

## 修改前审计：参考通道相关例外

### LILYGO T-CameraPlus-S3 V1.0/V1.1：明确的格式不一致

`config.h` 设置 `AUDIO_INPUT_REFERENCE=true`，codec 因而声明两通道 MR，但 `CreateVoiceHardware()` 将麦克风 RX 配置为 `I2S_SLOT_MODE_MONO`。`Read()` 直接读取单声道样本，没有插入播放参考；`Write()` 也没有建立软件回采。这样连续的麦克风样本会被上层按 M/R 成对解释，读取时长与上层帧长也不匹配。

这条路径应优先按实际单麦输入核对并改为 M，而不是继续统一 MR。V1.2 使用单声道 PDM 且声明 M，不受这个声明不一致影响。此处是独立源码发现，不能认定为 Box Lite issue 的根因。

依据：[配置](../main/boards/lilygo/t-cameraplus-s3/config.h)、[codec](../main/boards/lilygo/t-cameraplus-s3/tcamerapluss3_audio_codec.cc)。

### LILYGO T-Circle-S3 / T-Display-S3-Pro-MVSrLora：参考来源待硬件核实

两者声明 MR，直接从立体声麦克风 I2S RX 读取，没有软件插入播放参考。源码能够确认两通道布局，但不能证明第二个 slot 接入了播放回采；需核对原理图或录音。不可仅凭 `input_reference=true` 判定其 AEC 参考有效，也不能在未核对硬件时直接修改 slot/pin。

依据：[T-Circle](../main/boards/lilygo/t-circle-s3/tcircles3_audio_codec.cc)、[T-Display](../main/boards/lilygo/t-display-s3-pro-mvsrlora/tdisplays3promvsrlora_audio_codec.cc)。逐板表中 † 表示上述不一致或参考来源待核实。

### 定义了参考宏，但实际 codec 仍是 M

例如 M5Stack Atom EchoS3R、AtomS3R Echo Base 等目录定义 `AUDIO_INPUT_REFERENCE=true`，但实例化的是不接受该参数的 `Es8311AudioCodec`。该类固定 `input_reference_=false`、`input_channels_=1`。改宏不会自动产生回采。审计以实际构造参数和 codec 实现为准。

### 软件参考与硬件参考分开验证

Box Lite 与 AIVOX3 在 Read/Write 之间使用软件缓冲构造参考；BoxAudioCodec 等实现从选定 ADC/I2S 通道读取参考。统一为 MR 仅统一布局，不会统一两者的时序和回声消除效果。

Box Lite 的共享参考缓冲及读写位置没有并发保护，采集错误仍返回成功，属于另外需要验证和修复的问题；不能以单麦切换代替这些检查。

## 后续硬件验证

1. Box Lite：默认单麦 MR/M 与显式双麦 MMR/MM 对照，验证唤醒、通话、播放后再收音、打断和重连，以及设备端 AEC 开/关。单麦配置不能代替软件参考时序及并发检查。
2. T-CameraPlus-S3：验证 V1.0/V1.1 的单声道采集与 M 声明，以及 V1.2 的回归。
3. S31 Korvo-1：验证默认 M 采集与立体声播放；当前没有该板原故障的复现证据，不能宣称已修复该故障。
4. 核对两款 LILYGO 的参考来源。无参考的板子若要新增 MR，必须实际提供并对齐参考数据，不能只改通道数或格式字符串。

前面对 ESP-SR 内部输出路径的分析仍是待验证假设，不作为故障已修复的依据。本轮没有实机验证。

## 本轮验证记录

- 主机测试：`python3 -m unittest discover -s scripts/tests -v`，81 项通过。
- 格式检查：4 个修改的 C/C++ 文件通过仓库配置的 clang-format 检查；差异空白检查保留 Box Lite 原有 CRLF 行尾。
- Box Lite / T-CameraPlus-S3 V1.0/V1.1：按规范调用 `scripts/build.py`，ESP-IDF 6.1 在依赖配置阶段被阻断，报 `ESP_VIDEO_USE_CUSTOMIZED_ESP_H264_VERSION` 不存在。Box Lite 使用受支持的 6.0.2 重试也被配置检查阻断（`Missing required kconfig option after retry`）。这两款未完成固件编译。
- ESP32-S31-Korvo-1：ESP-IDF `v6.1-beta1-701-g812f3e98ca5`，规范构建和固件合并通过。当前本地构建目录对应此板型。
- 没有进行物理硬件测试；双麦实验路径尚未经实机验证。

## 修改前逐板清单

多个 codec 表示编译选项、硬件版本或运行时选择。格式逐项标注，避免把同目录的不同路径合并为一个结论。

| 板级目录 | codec → 声明格式 |
|---|---|
| [alientek/atk-dnesp32s3](../main/boards/alientek/atk-dnesp32s3/) | `Es8388AudioCodec` → M |
| [alientek/atk-dnesp32s3-box](../main/boards/alientek/atk-dnesp32s3-box/) | `ATK_NoAudioCodecDuplex` → M<br>`Es8311AudioCodec` → M |
| [alientek/atk-dnesp32s3-box0](../main/boards/alientek/atk-dnesp32s3-box0/) | `Es8311AudioCodec` → M |
| [alientek/atk-dnesp32s3-box2-4g](../main/boards/alientek/atk-dnesp32s3-box2-4g/) | `Es8389AudioCodec` → M |
| [alientek/atk-dnesp32s3-box2-wifi](../main/boards/alientek/atk-dnesp32s3-box2-wifi/) | `Es8389AudioCodec` → M |
| [alientek/atk-dnesp32s3-box3](../main/boards/alientek/atk-dnesp32s3-box3/) | `BoxAudioCodec` → MR |
| [alientek/atk-dnesp32s3m-4g](../main/boards/alientek/atk-dnesp32s3m-4g/) | `Es8388AudioCodec` → M |
| [alientek/atk-dnesp32s3m-wifi](../main/boards/alientek/atk-dnesp32s3m-wifi/) | `Es8388AudioCodec` → M |
| [bread-compact-esp32](../main/boards/bread-compact-esp32/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [bread-compact-esp32-lcd](../main/boards/bread-compact-esp32-lcd/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [bread-compact-ml307](../main/boards/bread-compact-ml307/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [bread-compact-nt26](../main/boards/bread-compact-nt26/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [bread-compact-wifi](../main/boards/bread-compact-wifi/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [bread-compact-wifi-lcd](../main/boards/bread-compact-wifi-lcd/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [bread-compact-wifi-s3cam](../main/boards/bread-compact-wifi-s3cam/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [dfrobot/df-k10](../main/boards/dfrobot/df-k10/) | `K10AudioCodec` → MR |
| [dfrobot/df-s3-ai-cam](../main/boards/dfrobot/df-s3-ai-cam/) | `NoAudioCodecSimplexPdm` → M |
| [doit-s3-aibox](../main/boards/doit-s3-aibox/) | `NoAudioCodecSimplexPdm` → M |
| [du-chatx](../main/boards/du-chatx/) | `NoAudioCodecSimplex` → M |
| [electron-bot](../main/boards/electron-bot/) | `NoAudioCodecSimplex` → M |
| [espressif/esp-hi](../main/boards/espressif/esp-hi/) | `AdcPdmAudioCodec` → M |
| [espressif/esp-sensairshuttle](../main/boards/espressif/esp-sensairshuttle/) | `AdcPdmAudioCodec` → M |
| [espressif/esp-sparkbot](../main/boards/espressif/esp-sparkbot/) | `SparkBotEs8311AudioCodec` → M |
| [espressif/esp-spot](../main/boards/espressif/esp-spot/) | `Es8311AudioCodec` → M |
| [espressif/esp-vocat](../main/boards/espressif/esp-vocat/) | `BoxAudioCodec` → MR |
| [espressif/esp32-p4-function-ev-board](../main/boards/espressif/esp32-p4-function-ev-board/) | `Es8311AudioCodec` → M |
| [espressif/esp32-s3-box](../main/boards/espressif/esp32-s3-box/) | `BoxAudioCodec` → MR |
| [espressif/esp32-s3-box-3](../main/boards/espressif/esp32-s3-box-3/) | `BoxAudioCodec` → MR |
| [espressif/esp32-s3-box-lite](../main/boards/espressif/esp32-s3-box-lite/) | `BoxAudioCodecLite` → MMR / MM（随音频处理开关） |
| [espressif/esp32-s3-korvo-2-v3.0](../main/boards/espressif/esp32-s3-korvo-2-v3.0/) | `BoxAudioCodec` → MR |
| [espressif/esp32-s3-korvo-2-v3.0-rndis](../main/boards/espressif/esp32-s3-korvo-2-v3.0-rndis/) | `BoxAudioCodec` → MR |
| [espressif/esp32-s3-lcd-ev-board](../main/boards/espressif/esp32-s3-lcd-ev-board/) | `BoxAudioCodec` → MR |
| [espressif/esp32-s3-lcd-ev-board-2](../main/boards/espressif/esp32-s3-lcd-ev-board-2/) | `BoxAudioCodec` → MR |
| [espressif/esp32-s31-function-coreboard-1](../main/boards/espressif/esp32-s31-function-coreboard-1/) | `Es8311AudioCodec` → M |
| [espressif/esp32-s31-korvo-1](../main/boards/espressif/esp32-s31-korvo-1/) | `Es8389AudioCodec` → MM |
| [folotoy/ai-passport](../main/boards/folotoy/ai-passport/) | `Es8311AudioCodec` → M |
| [freenove-esp32s3-display-2.8-lcd](../main/boards/freenove-esp32s3-display-2.8-lcd/) | `Es8311AudioCodec` → M |
| [genjutech-s3-1.54tft](../main/boards/genjutech-s3-1.54tft/) | `SparkBotEs8311AudioCodec` → M |
| [hu-087](../main/boards/hu-087/) | `NoAudioCodecSimplex` → M |
| [jiuchuan-s3](../main/boards/jiuchuan-s3/) | `Es8311AudioCodec` → M |
| [kevin/box-2](../main/boards/kevin/box-2/) | `BoxAudioCodec` → MR |
| [kevin/c3](../main/boards/kevin/c3/) | `Es8311AudioCodec` → M |
| [kevin/sp-v3-dev](../main/boards/kevin/sp-v3-dev/) | `NoAudioCodecSimplex` → M |
| [kevin/sp-v4-dev](../main/boards/kevin/sp-v4-dev/) | `Es8311AudioCodec` → M |
| [kevin/yuying-313lcd](../main/boards/kevin/yuying-313lcd/) | `Es8311AudioCodec` → M |
| [labplus/ledong-v2](../main/boards/labplus/ledong-v2/) | `Es8388AudioCodec` → M |
| [labplus/mpython-v3](../main/boards/labplus/mpython-v3/) | `Es8388AudioCodec` → M |
| [lcdwiki-es3c35p](../main/boards/lcdwiki-es3c35p/) | `Es8311AudioCodec` → M |
| [lceda-course-examples/eda-robot-pro](../main/boards/lceda-course-examples/eda-robot-pro/) | `NoAudioCodecSimplex` → M |
| [lceda-course-examples/eda-super-bear](../main/boards/lceda-course-examples/eda-super-bear/) | `NoAudioCodecSimplex` → M |
| [lceda-course-examples/eda-tv-pro](../main/boards/lceda-course-examples/eda-tv-pro/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [lckfb/szpi-esp32c3](../main/boards/lckfb/szpi-esp32c3/) | `Es8311AudioCodec` → M |
| [lckfb/szpi-esp32s3](../main/boards/lckfb/szpi-esp32s3/) | `CustomAudioCodec` → MR |
| [lilygo/t-cameraplus-s3](../main/boards/lilygo/t-cameraplus-s3/) | `Tcamerapluss3AudioCodec` → MR†（V1.0/V1.1）/ M（V1.2） |
| [lilygo/t-circle-s3](../main/boards/lilygo/t-circle-s3/) | `Tcircles3AudioCodec` → MR† |
| [lilygo/t-display-p4](../main/boards/lilygo/t-display-p4/) | `Es8311AudioCodec` → M |
| [lilygo/t-display-s3-pro-mvsrlora](../main/boards/lilygo/t-display-s3-pro-mvsrlora/) | `Tdisplays3promvsrloraAudioCodec` → MR† |
| [m5stack/atom-echos3r](../main/boards/m5stack/atom-echos3r/) | `Es8311AudioCodec` → M |
| [m5stack/atommatrix-echo-base](../main/boards/m5stack/atommatrix-echo-base/) | `Es8311AudioCodec` → M |
| [m5stack/atoms3-echo-base](../main/boards/m5stack/atoms3-echo-base/) | `Es8311AudioCodec` → M |
| [m5stack/atoms3r-cam-m12-echo-base](../main/boards/m5stack/atoms3r-cam-m12-echo-base/) | `Es8311AudioCodec` → M |
| [m5stack/atoms3r-echo-base](../main/boards/m5stack/atoms3r-echo-base/) | `Es8311AudioCodec` → M |
| [m5stack/atoms3r-echo-pyramid](../main/boards/m5stack/atoms3r-echo-pyramid/) | `PyramidAudioCodec` → MR |
| [m5stack/cardputer-adv](../main/boards/m5stack/cardputer-adv/) | `CardputerAdvEs8311` → M |
| [m5stack/core-s3](../main/boards/m5stack/core-s3/) | `CoreS3AudioCodec` → M |
| [m5stack/corep4](../main/boards/m5stack/corep4/) | `BoxAudioCodec` → MR |
| [m5stack/stick-s3](../main/boards/m5stack/stick-s3/) | `Sticks3AudioCodec` → M |
| [m5stack/stopwatch](../main/boards/m5stack/stopwatch/) | `Es8311AudioCodec` → M |
| [m5stack/tab5](../main/boards/m5stack/tab5/) | `Tab5AudioCodec` → MR |
| [magiclick/2p4](../main/boards/magiclick/2p4/) | `Es8311AudioCodec` → M |
| [magiclick/2p5](../main/boards/magiclick/2p5/) | `Es8311AudioCodec` → M |
| [magiclick/c3](../main/boards/magiclick/c3/) | `Es8311AudioCodec` → M |
| [magiclick/c3-v2](../main/boards/magiclick/c3-v2/) | `Es8311AudioCodec` → M |
| [maza-ai/esp32-s3](../main/boards/maza-ai/esp32-s3/) | `Es8311AudioCodec` → M |
| [maza-ai/esp32-s31](../main/boards/maza-ai/esp32-s31/) | `Es8311AudioCodec` → M |
| [minsi-k08-dual](../main/boards/minsi-k08-dual/) | `NoAudioCodecSimplex` → M |
| [mixgo-nova](../main/boards/mixgo-nova/) | `Es8374AudioCodec` → M |
| [movecall/cuican-esp32s3](../main/boards/movecall/cuican-esp32s3/) | `Es8311AudioCodec` → M |
| [movecall/moji-esp32s3](../main/boards/movecall/moji-esp32s3/) | `Es8311AudioCodec` → M |
| [movecall/moji2-esp32c5](../main/boards/movecall/moji2-esp32c5/) | `Es8311AudioCodec` → M |
| [nologo/xingzhi-abs-2.0](../main/boards/nologo/xingzhi-abs-2.0/) | `Es8311AudioCodec` → M |
| [nologo/xingzhi-cube-0.85tft-ml307](../main/boards/nologo/xingzhi-cube-0.85tft-ml307/) | `NoAudioCodecSimplex` → M |
| [nologo/xingzhi-cube-0.85tft-wifi](../main/boards/nologo/xingzhi-cube-0.85tft-wifi/) | `NoAudioCodecSimplex` → M |
| [nologo/xingzhi-cube-0.96oled-ml307](../main/boards/nologo/xingzhi-cube-0.96oled-ml307/) | `NoAudioCodecSimplex` → M |
| [nologo/xingzhi-cube-0.96oled-wifi](../main/boards/nologo/xingzhi-cube-0.96oled-wifi/) | `NoAudioCodecSimplex` → M |
| [nologo/xingzhi-cube-1.54tft-ml307](../main/boards/nologo/xingzhi-cube-1.54tft-ml307/) | `NoAudioCodecSimplex` → M |
| [nologo/xingzhi-cube-1.54tft-wifi](../main/boards/nologo/xingzhi-cube-1.54tft-wifi/) | `NoAudioCodecSimplex` → M |
| [nologo/xingzhi-metal-1.54-wifi](../main/boards/nologo/xingzhi-metal-1.54-wifi/) | `Es8311AudioCodec` → M |
| [nulllab-ai-vox-v3](../main/boards/nulllab-ai-vox-v3/) | `AIVOX3AudioCodec` → MR / M（随设备 AEC 开关） |
| [otto-robot](../main/boards/otto-robot/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [quandong-s3-dev](../main/boards/quandong-s3-dev/) | `Es8311AudioCodec` → M |
| [rymcu/bigsmart](../main/boards/rymcu/bigsmart/) | `CustomAudioCodec` → MR |
| [sensecap-watcher](../main/boards/sensecap-watcher/) | `SensecapAudioCodec` → M |
| [spotpear/sp-esp32-s3-1.28-box](../main/boards/spotpear/sp-esp32-s3-1.28-box/) | `Es8311AudioCodec` → M |
| [spotpear/sp-esp32-s3-1.54-muma](../main/boards/spotpear/sp-esp32-s3-1.54-muma/) | `Es8311AudioCodec` → M |
| [surfer-c3-1.14tft](../main/boards/surfer-c3-1.14tft/) | `Es8311AudioCodec` → M |
| [taiji-pi-s3](../main/boards/taiji-pi-s3/) | `NoAudioCodecSimplex` → M<br>`NoAudioCodecSimplexPdm` → M |
| [waveshare/esp32-c5-touch-lcd-1.69](../main/boards/waveshare/esp32-c5-touch-lcd-1.69/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-c6-epaper-1.54](../main/boards/waveshare/esp32-c6-epaper-1.54/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-c6-lcd-0.85](../main/boards/waveshare/esp32-c6-lcd-0.85/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-c6-lcd-1.69](../main/boards/waveshare/esp32-c6-lcd-1.69/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-c6-touch-amoled-1.32](../main/boards/waveshare/esp32-c6-touch-amoled-1.32/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-c6-touch-amoled-1.43](../main/boards/waveshare/esp32-c6-touch-amoled-1.43/) | `BoxAudioCodec` → M |
| [waveshare/esp32-c6-touch-amoled-1.8](../main/boards/waveshare/esp32-c6-touch-amoled-1.8/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-c6-touch-amoled-2.06](../main/boards/waveshare/esp32-c6-touch-amoled-2.06/) | `BoxAudioCodec` → M |
| [waveshare/esp32-c6-touch-amoled-2.16](../main/boards/waveshare/esp32-c6-touch-amoled-2.16/) | `BoxAudioCodec` → M |
| [waveshare/esp32-c6-touch-lcd-1.54](../main/boards/waveshare/esp32-c6-touch-lcd-1.54/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-c6-touch-lcd-1.83](../main/boards/waveshare/esp32-c6-touch-lcd-1.83/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-p4-nano](../main/boards/waveshare/esp32-p4-nano/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-p4-wifi6-touch-lcd](../main/boards/waveshare/esp32-p4-wifi6-touch-lcd/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-p4-wifi6-touch-lcd-3.5](../main/boards/waveshare/esp32-p4-wifi6-touch-lcd-3.5/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-s3-audio-board](../main/boards/waveshare/esp32-s3-audio-board/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-cam](../main/boards/waveshare/esp32-s3-cam/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-epaper-1.54](../main/boards/waveshare/esp32-s3-epaper-1.54/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-s3-epaper-3.97](../main/boards/waveshare/esp32-s3-epaper-3.97/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-s3-lcd-0.85](../main/boards/waveshare/esp32-s3-lcd-0.85/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-rgb-matrix](../main/boards/waveshare/esp32-s3-rgb-matrix/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-rlcd-4.2](../main/boards/waveshare/esp32-s3-rlcd-4.2/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-amoled-1.32](../main/boards/waveshare/esp32-s3-touch-amoled-1.32/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-s3-touch-amoled-1.43c](../main/boards/waveshare/esp32-s3-touch-amoled-1.43c/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-amoled-1.75](../main/boards/waveshare/esp32-s3-touch-amoled-1.75/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-amoled-1.8](../main/boards/waveshare/esp32-s3-touch-amoled-1.8/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-s3-touch-amoled-1.8-v2](../main/boards/waveshare/esp32-s3-touch-amoled-1.8-v2/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-s3-touch-amoled-2.06](../main/boards/waveshare/esp32-s3-touch-amoled-2.06/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-amoled-2.16](../main/boards/waveshare/esp32-s3-touch-amoled-2.16/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-lcd-1.46](../main/boards/waveshare/esp32-s3-touch-lcd-1.46/) | `NoAudioCodecSimplex` → M |
| [waveshare/esp32-s3-touch-lcd-1.54](../main/boards/waveshare/esp32-s3-touch-lcd-1.54/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-lcd-1.83](../main/boards/waveshare/esp32-s3-touch-lcd-1.83/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-lcd-1.85](../main/boards/waveshare/esp32-s3-touch-lcd-1.85/) | `NoAudioCodecSimplex` → M |
| [waveshare/esp32-s3-touch-lcd-1.85b](../main/boards/waveshare/esp32-s3-touch-lcd-1.85b/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-lcd-1.85c](../main/boards/waveshare/esp32-s3-touch-lcd-1.85c/) | `BoxAudioCodec` → MR<br>`NoAudioCodecSimplex` → M |
| [waveshare/esp32-s3-touch-lcd-3.49](../main/boards/waveshare/esp32-s3-touch-lcd-3.49/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-lcd-3.49-v2](../main/boards/waveshare/esp32-s3-touch-lcd-3.49-v2/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-lcd-3.5](../main/boards/waveshare/esp32-s3-touch-lcd-3.5/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-s3-touch-lcd-3.5b](../main/boards/waveshare/esp32-s3-touch-lcd-3.5b/) | `Es8311AudioCodec` → M |
| [waveshare/esp32-s3-touch-lcd-4.3c](../main/boards/waveshare/esp32-s3-touch-lcd-4.3c/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-lcd-4b](../main/boards/waveshare/esp32-s3-touch-lcd-4b/) | `BoxAudioCodec` → MR |
| [waveshare/esp32-s3-touch-lcd-7c](../main/boards/waveshare/esp32-s3-touch-lcd-7c/) | `WaveshareAudioCodec` → MR |
| [waveshare/esp32-touch-lcd-3.5](../main/boards/waveshare/esp32-touch-lcd-3.5/) | `Es8311AudioCodec` → M |
| [wdmomo/esp32-cgc](../main/boards/wdmomo/esp32-cgc/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [wdmomo/esp32-cgc-144](../main/boards/wdmomo/esp32-cgc-144/) | `NoAudioCodecSimplex` → M |
| [wk-esp32s3-dev](../main/boards/wk-esp32s3-dev/) | `NoAudioCodecDuplex` → M<br>`NoAudioCodecSimplex` → M |
| [xmini/c3](../main/boards/xmini/c3/) | `Es8311AudioCodec` → M |
| [xmini/c3-4g](../main/boards/xmini/c3-4g/) | `Es8311AudioCodec` → M |
| [xmini/c3-v3](../main/boards/xmini/c3-v3/) | `Es8311AudioCodec` → M |
| [xorigin/aipi-lite](../main/boards/xorigin/aipi-lite/) | `Es8311AudioCodec` → M |
| [yunliao-s3](../main/boards/yunliao-s3/) | `Es8388AudioCodec` → MR |
| [zectrix/zectrix-s3-epaper-4.2](../main/boards/zectrix/zectrix-s3-epaper-4.2/) | `Es8311AudioCodec` → M |
| [zhengchen/1.54tft-ml307](../main/boards/zhengchen/1.54tft-ml307/) | `NoAudioCodecSimplex` → M |
| [zhengchen/1.54tft-wifi](../main/boards/zhengchen/1.54tft-wifi/) | `NoAudioCodecSimplex` → M |
| [zhengchen/cam](../main/boards/zhengchen/cam/) | `CustomAudioCodec` → MR |
| [zhengchen/cam-ml307](../main/boards/zhengchen/cam-ml307/) | `CustomAudioCodec` → MR |
