# 编译配置命令

**配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> MolunSmartHome ESP32-S3-WROOM-1-N4
```

**修改 flash 大小：**

```
Serial flasher config -> Flash size -> 4 MB
```

**修改分区表：**

```
Partition Table -> Custom partition CSV file -> partitions_4M.csv
```

**修改通信协议为Websocket（效果会好些）：**

```
Xiaozhi Assistant -> Connection Type -> WebSocket
```

**关闭语音唤醒与音频处理：**

```
Xiaozhi Assistant -> USE_AUDIO_PROCESSING -> No
```

**关闭PSRAM：**

```
Component config -> ESP PSRAM -> Support for external, SPI-connected RAM -> No
```



**编译：**

```bash
idf.py build
```

![alt text](image.png)
![alt text](image-1.png)
![alt text](image-2.png)