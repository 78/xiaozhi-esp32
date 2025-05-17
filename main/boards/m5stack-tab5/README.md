# 使用说明 

* [M5Stack Tab5 docs](https://docs.m5stack.com/zh_CN/core/Tab5)

## 快速体验

下载编译好的 [固件](https://pan.baidu.com/s/1dgbUQtMyVLSCSBJLHARpwQ?pwd=1234) 提取码: 1234 

```shell
esptool.py --chip esp32p4 -p /dev/ttyACM0 -b 460800 --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x00 tab5_xiaozhi_v1_addr0.bin 
```

## 基础使用

* idf version: v5.5-dev

1. 设置编译目标为 esp32p4

```shell
idf.py set-target esp32p4 
```

2. 修改配置 

```shell
cp main/boards/m5stack-tab5/sdkconfig.tab5 sdkconfig
```

3. 编译烧录程序

```shell
idf.py build flash monitor
```

> [!NOTE]
> 进入下载模式：长按复位按键（约 2 秒），直至内部绿色 LED 指示灯开始快速闪烁，松开按键。


## log

@2025/05/17 测试问题

1. listening... 需要等几秒才能获取语音输入???
2. 亮度调节不对
3. 音量调节不对
 
## TODO
