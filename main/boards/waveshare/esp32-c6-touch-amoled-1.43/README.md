# 产品链接

[微雪电子 ESP32-C6-Touch-AMOLED-1.43](https://www.waveshare.net/shop/ESP32-C6-Touch-AMOLED-1.43.htm)
[微雪电子 ESP32-C6-Touch-AMOLED-1.43-B](https://www.waveshare.net/shop/ESP32-C6-Touch-AMOLED-1.43-B.htm)

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
Xiaozhi Assistant -> Board Type -> Waveshare ESP32-C6-Touch-AMOLED-1.43
```

**编译**

```ba
idf.py build
```

**下载并打开串口终端**

```bash
idf.py build flash monitor
```

