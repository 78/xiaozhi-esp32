# StackChan 使用说明

StackChan 是石川真也（Shinya Ishikawa）开源的桌面机器人，M5Stack 以 K151 / K151-R 出货，
底座带一对 Feetech SCS 串行总线伺服作脖子。它跑在 M5Stack CoreS3 上，所以音频编解码直接复用
隔壁 `m5stack/core-s3/cores3_audio_codec.cc`，没有再抄一份。

**硬件**

| | |
|---|---|
| 主控 | M5Stack CoreS3（ESP32-S3，Octal PSRAM 关、Quad 开） |
| 显示 | ILI9342C，320×240，走 SPI；表情用 emote 动画 |
| 音频 | AW88298 功放 + ES7210 回采，和 CoreS3 一致 |
| 电源 | AXP2101 |
| 脖子 | 两个 Feetech SCS0009，串行总线，1 Mbaud |
| 底座 | PY32 I/O 扩展器，第 0 脚是伺服总电源（VM EN） |

> [!IMPORTANT]
> 伺服电源由底座的 PY32 第 0 脚控制，上电默认是断的。不先拉高这一脚，总线能发能收、
> 舵机也能报回位置，但**完全不会动** —— 这一条是这块板子最容易踩的坑。
> 拉高之后还要等约 200 ms，SCS0009 才醒。见 `EnableServoPower()`。

**编译**

```bash
python ./scripts/build.py m5stack/stackchan
```

如需手动编译，请参考 `m5stack/stackchan/config.json` 修改 menuconfig 对应选项。

**烧录**

```bash
idf.py flash
```

> [!NOTE]
> 进入下载模式：长按复位按键（约 3 秒），直至内部指示灯亮绿色，松开按键。

**脖子工具**

| 工具 | 作用 |
|---|---|
| `self.neck.get_angles` | 读当前偏航 / 俯仰角，以及总线有没有应答 |
| `self.neck.set_angles` | 设目标角，带缓动时长 |
| `self.neck.set_torque` | 开关舵机扭矩 |

角度以中立姿态为零点，抬头为正。

闲置时自己小幅摆动、触摸、灯带、IMU、光照传感器这些另算，不在这块板子里。
