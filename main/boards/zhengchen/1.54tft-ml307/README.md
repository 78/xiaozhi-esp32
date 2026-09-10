# 产品相关介绍网址

```http
https://e.tb.cn/h.6Gl2LC7rsrswQZp?tk=qFuaV9hzh0k CZ356
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
Xiaozhi Assistant -> Board Type -> zhengchen-1.54tft-ml307
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
