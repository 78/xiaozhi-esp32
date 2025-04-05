# 编译配置命令

**配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32c3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> EMO Dot 小豆机器人 C3 1.28
```

**修改 flash 大小：**

```
Serial flasher config -> Flash size -> 4 MB
```

**修改分区表：**

```
Partition Table -> Custom partition CSV file -> partitions_4M_Supermini_C3.csv
```

**修改 console 默认输出：**

```
Component config -> ESP System Setting -> Channel for console output -> USB Serial/JTAG Controller
Component config -> ESP System Setting -> Channel for console secondary output -> No secondary console
```

**修改 WiFi 功率：**
```
Component config -> PHY  -> Max WiFi TX power  -> 10
```

**编译：**

```bash
idf.py build
```