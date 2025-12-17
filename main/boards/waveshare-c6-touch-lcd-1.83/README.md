
# 产品链接
ESP32-C6-Touch-LCD-1.83 is a high-performance and highly integrated microcontroller development board designed by Waveshare. With a compact board size, it is equipped with a 1.83-inch capacitive LCD screen, a highly integrated power management chip, six-axis sensors (three-axis accelerometer and three-axis gyroscope), RTC and low-power audio codec chip, etc., making it convenient for development and easy to be embedded in products.

[微雪电子 ESP32-C6-Touch-LCD-1.83](https://www.waveshare.net/shop/ESP32-C6-Touch-LCD-1.83.htm)

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
Xiaozhi Assistant -> Board Type -> Waveshare ESP32-C6-Touch-LCD-1.83
```

**编译**

```ba
idf.py build
```

**下载并打开串口终端**

```bash
idf.py build flash monitor
```
