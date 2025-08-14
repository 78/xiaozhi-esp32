# 产品链接

[微雪电子 ESP32-C6-Touch-LCD-1.69](https://www.waveshare.net/shop/ESP32-C6-Touch-LCD-1.69.htm)
[微雪电子 ESP32-C6-LCD-1.69](https://www.waveshare.net/shop/ESP32-C6-LCD-1.69.htm)

# 编译配置命令

**克隆工程**

```bash
git clone https://github.com/78/xiaozhi-esp32.git
```

**进入工程**

```bash
cd xiaozhi-esp32
```

**配置编译目标为 ESP32C6**

```bash
idf.py set-target esp32c6
```

**打开 menuconfig**

```bash
idf.py menuconfig
```

**选择板子**

```bash
Xiaozhi Assistant -> Board Type -> Waveshare ESP32-C6-LCD-1.69
```

**编译**

```ba
idf.py build
```

**下载并打开串口终端**

```bash
idf.py build flash monitor
```
# 按键操作
## BOOT 按键
**未连接服务器前单击: 进入配网模式**
**连接服务器后单击: 唤醒、打断**

## PWR 按键
**双击：息屏、亮屏**
**长按：开关机**