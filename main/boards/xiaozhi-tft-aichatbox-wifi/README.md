# 产品相关介绍网址

```http
https://shop41675499.m.youzan.com/wscgoods/detail/3nwov137enbkzy5?scan=1&activity=none&from=kdt&qr=directgoods_3931018239&shopAutoEnter=1
```

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
Xiaozhi Assistant -> Board Type -> AiChatBox-WiFi
```

```

**编译：**

bash
idf.py build
```

**下载：**
idf.py build flash monitor

进行下载和显示日志


**固件生成：**

```bash
idf.py merge-bin
```
