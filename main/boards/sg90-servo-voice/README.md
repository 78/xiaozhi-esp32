# SG90 舵机语音控制板

## 概述

这是一个小智语音控制 SG90 的示例代码，支持通过语音指令控制舵机的角度和运动。

## 硬件配置

硬件配置和小智 AI 的面包接线方式完全相同。 可以参考 [小智 AI 聊天机器人面包板 DIY 硬件清单与接线教程](https://ccnphfhqs21z.feishu.cn/wiki/EH6wwrgvNiU7aykr7HgclP09nCh)
**唯一区别是需要连接一个 SG90 舵机**

舵机一般有三根线：

| 颜色   | 含义           |
| ------ | -------------- |
| 橙色   | 表示信号线     |
| 红色   | 表示电源正极线 |
| 咖啡色 | 表示电源负极线 |

舵机和开发板的接线方式如下

| ESP32S3 开发板 | SG90 舵机    |
| -------------- | ------------ |
| GPIO18         | 橙色信号线   |
| 3V3 电源       | 红色正极线   |
| GND            | 咖啡色负极线 |

### MCP 工具列表

- `self.servo.set_angle` - 设置舵机角度
- `self.servo.rotate_clockwise` - 顺时针旋转
- `self.servo.rotate_counterclockwise` - 逆时针旋转
- `self.servo.get_position` - 获取当前位置
- `self.servo.stop` - 停止舵机
- `self.servo.sweep` - 扫描模式
- `self.servo.reset` - 复位到中心位置

## 编译和烧录

编译和烧录有两种方式，一种是直接编译出固件(一般在发布阶段使用)，一种是手动编译和烧录（一般在开发阶段使用）

### 方式一：直接编译出固件

**第一步：设置 ESP-IDF 环境**:

```bash
source /path/to/esp-idf/export.sh
```

**第二步：编译固件**:

```bash
python scripts/release.py sg90-servo-voice
```

提示：生成的固件文件在: `releases/v1.8.1_sg90-servo-voice.zip`

### 方式二：手动编译和烧录

**第一步：设置 ESP-IDF 环境**:

```bash
source /path/to/esp-idf/export.sh
```

**第二步：设置芯片**

```bash
idf.py set-target esp32s3
```

**第三步：设置开发板**

```bash
idf.py menuconfig

# 然后依次选择：Xiaozhi Assistant -> Board Type -> SG90舵机语音控制板（一般在最后一个）
```

**第三步：编译固件**

```bash
idf.py build
```

**第三步：烧录与监控器**

```bash
idf.py flash monitor
```

### 语音控制

设备配置完成后，可以使用以下语音指令试一下：

- **"设置舵机角度到 90 度"** - 设置舵机到指定角度
- **"顺时针旋转 45 度"** - 顺时针旋转指定角度
- **"逆时针旋转 30 度"** - 逆时针旋转指定角度
- **"开始扫描模式"** - 在 0-180 度范围内来回摆动
- **"停止舵机"** - 立即停止舵机运动
- **"复位舵机"** - 回到中心位置（90 度）
- **"查看舵机状态"** - 获取当前角度和运动状态
