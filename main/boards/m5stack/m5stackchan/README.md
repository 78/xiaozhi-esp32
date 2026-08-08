# M5Stack StackChan (K151)

官方 M5StackChan AI 桌面机器人：CoreS3 主机 + 舵机机身。

主机部分（屏幕、触摸、摄像头、音频、AXP2101 电源）与 `m5stack/core-s3` 一致，
本板在其基础上增加机身外设、动画表情和状态灯。

## 机身硬件

- **Feetech SCS0009 串行总线舵机**（UART1，1 Mbps 8N1，TX=GPIO6 / RX=GPIO7）
  - Yaw（水平）ID=1，零位 raw 460，可动范围 ±128°
  - Pitch（俯仰）ID=2，零位 raw 620，可动范围 0~85°（0° 为水平）
  - 角度换算 `raw = zero + 角度 × 16 / 5`，再钳位到 0~1000
- **PY32L020 IO 扩展**（内部 I2C 0x6F，**只能跑 100 kHz**）
  - pin 0 = `VM_EN` 舵机电源使能，**不拉高舵机不会动**
  - pin 13 = RGB 使能，机身共 12 颗灯
- 机身其余 I2C 设备（未使用）：INA226 电池监测 0x41、NFC 0x50、Si12T 触摸 0x68

> [!CAUTION]
> M5Stack 官方说明俯仰舵机在机械极限处会堵转并永久损坏，因此固件把可动范围
> 限制在 85° 以内。修改 `SERVO_PITCH_MAX_DEGREE` 前请确认清楚。

## 显示

采用 `USE_EMOTE_MESSAGE_STYLE` 全屏动画表情（320×240，20 fps），
不显示对话字幕。资源来自 `esp_emote_assets` 组件。

该组件自带的 assets 分区里打包了它自己的唤醒词模型，会把唤醒词改成「你好喵伴」。
为保留项目默认的「你好小智」，`main/CMakeLists.txt` 通过 `EMOTE_EXTERNAL_PATH`
扩展点，在构建时从 `esp-sr` 组件取模型重新打包并覆盖：

```
build/m5stackchan-assets/wakenet/srmodels.bin   ← 构建时生成，优先于组件自带
```

打包脚本按「先查 external、再回退组件」的顺序取资源，所以动画、字体、布局
仍然全部使用组件默认值。若 `esp-sr` 路径缺失，构建只告警并回退到组件唤醒词。

切回静态 emoji：删除 `config.json` 里的 `CONFIG_USE_EMOTE_MESSAGE_STYLE=y`
即可，此时改用 `SpiLcdDisplay` 并隐藏字幕栏。

## 状态灯

机身 12 颗 RGB 灯通过 `Board::GetLed()` 挂到设备状态上，亮度 96/255：

| 状态 | 颜色 |
| --- | --- |
| 聆听 | 绿 |
| 说话 | 蓝 |
| 启动 / 配网 / 连接 / 激活 / 升级 | 暖黄 |
| 严重错误 | 红 |
| 空闲 | 灭 |

`self.body.set_rgb_color` 设置的颜色会在下一次状态切换时被覆盖。

## MCP 工具

| 工具 | 说明 |
| --- | --- |
| `self.head.set_pose` | 转到绝对姿态（yaw / pitch / duration_ms） |
| `self.head.home` | 回正 |
| `self.head.nod` | 点头 |
| `self.head.shake` | 摇头 |
| `self.body.set_rgb_color` | 设置机身 12 颗 RGB 灯颜色 |

动作在独立任务的队列里串行执行并阻塞等待完成，MCP 回调只入队，不会阻塞主事件循环。

## 编译

```bash
python3 scripts/build.py m5stack/m5stackchan --name m5stackchan
```

## 烧录

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

> [!NOTE]
> 进入下载模式：长按底座旁的 RST 按键约 3 秒，指示灯变绿后松开。
> 烧录完成后需**短按一次 RST** 才会退出下载模式启动固件。
> 建议使用**底座上的 USB-C 口**，避免舵机转动带来的意外。

> [!WARNING]
> 刷入本固件会覆盖出厂固件。在此之前请先在 StackChan World App 或出厂固件的
> Setup 菜单里**解绑设备**，否则在 xiaozhi.me 上配对会失败。
> 恢复出厂固件请使用 M5Burner。

## 排查

开机日志中应出现：

```
BodyIoExpander: Body IO expander ready after N ms, version 0x41
ScsServoBus: SCS servo bus ready on UART1 (tx=6, rx=7, 1000000 bps)
```

第一条缺失说明机身 I2C 未连通，此时 `VM_EN` 未使能、舵机不会动，
RGB 工具也不会注册；固件其余部分仍可正常运行。
板子构造函数会打印一次 I2C 扫描表，机身在位时应能看到 `41 50 68 6f`。

