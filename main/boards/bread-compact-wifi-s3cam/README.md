硬件基于基于ESP32S3CAM开发板，代码基于bread-compact-wifi-lcd修改
使用的摄像头是OV2640
注意因为摄像头占用IO较多，所以占用了ESP32S3的USB 19 20两个引脚
连线方式参考config.h文件中对引脚的定义

 
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
Xiaozhi Assistant -> Board Type ->面包板新版接线（WiFi）+ LCD + Camera
```

**编译烧入：**

```bash
idf.py build flash
```