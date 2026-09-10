# 编译配置指南

本文档介绍了如何为 **Movecall Moji2.0 (小智AI衍生版)** 配置和编译固件。

## 🛠 环境要求
*   **ESP-IDF 版本**: v5.5
*   **芯片型号**: ESP32-C5

## 🔗 硬件开源信息
本项目基于以下开源硬件设计：
*   **立创开源硬件平台**: [https://oshwhub.com/movecall/moji2](https://oshwhub.com/movecall/moji2)

---

## 🚀 编译步骤

### 1. 设置编译目标
首先，将项目目标芯片设置为 ESP32-C5：
```bash
idf.py set-target esp32c5
```

### 2. 配置开发板型号
运行以下命令打开配置菜单进行板型选择：
```bash
idf.py menuconfig
```

**请在菜单中按照以下路径进行操作：**
> **Xiaozhi Assistant** -> **Board Type** -> **Movecall Moji2.0 小智AI衍生版**

*操作提示：配置完成后，按 **S** 保存并按回车确认，按 **Q** 退出。*

### 3. 执行编译
运行以下命令开始构建项目：
```bash
idf.py build
```

---

## 🔧 常用维护命令

**清理编译缓存 (遇到报错建议执行)：**
```bash
idf.py fullclean
```

**烧录固件：**
```bash
idf.py flash
```

**查看串口日志：**
```bash
idf.py monitor
```