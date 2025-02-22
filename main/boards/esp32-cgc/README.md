# 主板开源地址：
[https://oshwhub.com/wdmomo/esp32-xiaozhi-kidpcb](https://oshwhub.com/wdmomo/esp32-xiaozhi-kidpcb)

# 编译配置命令

**配置编译目标为 ESP32：**

```bash
idf.py set-target esp32
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> ESP32 CGC
```

**选择屏幕类型：**

```
Xiaozhi Assistant -> LCD Type -> "ST7735, 分辨率128*128"
```

**修改 flash 大小：**

```
Serial flasher config -> Flash size -> 4 MB
```

**修改分区表：**

```
Partition Table -> Custom partition CSV file -> partitions_4M.csv
```

**编译：**

```bash
idf.py build
```
