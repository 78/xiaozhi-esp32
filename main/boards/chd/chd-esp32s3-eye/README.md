# chd-esp32s3-eye

## 简介
虫洞esp32s3-eye开发板，搭载 ESP32-S3-WROOM-1 模组，配置1.54寸屏幕.

## 配置、编译命令

**配置编译目标为 ESP32S3**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig 并配置**

```bash
idf.py menuconfig
```

分别配置如下选项：

### 基本配置
- `Xiaozhi Assistant` → `Board Type` → 选择 `虫洞 esp32s3-EYE`


### 唤醒词配置

支持多种唤醒词实现方式：

- `Xiaozhi Assistant` → `Wake Word Implementation Type` → 选择唤醒词类型

按 `S` 保存，按 `Q` 退出。

**编译**

```bash
idf.py build
```

**烧录**

将 开发板 连接至电脑，并运行：

```bash
idf.py flash
```

## 按键说明

### Boot 按键功能

#### 单击
- **配网状态**: 进入 WiFi 配置模式
- **空闲状态**: 开始对话
- **对话中**: 打断或停止当前对话

 #### 合并固件命令 idf.py merge-bin
