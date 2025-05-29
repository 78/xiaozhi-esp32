
minsi-k08-wifi和minsi-k08-ml307是敏思科技推出的基于ESP32S3N16R8，搭载MAX98357音频功率放大器和INMP441全向麦克风模块，通过改造K08透明机甲小钢炮音箱而成的带有朋克风格的大喇叭大电池小智AI聊天机器人方案。

<a href="https://item.taobao.com/item.htm?id=889892765588" target="_blank" title="SenseCAP Watcher">Minsi-k08</a>

  <a href="minsi-k08.jpg" target="_blank" title="Minsi-k08">
    <img src="minsi-k08.jpg" width="240" />
  </a>



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
Xiaozhi Assistant -> Board Type ->敏思科技K08(DUAL)
```

**编译烧入：**

```bash
idf.py build flash
```