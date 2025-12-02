# 使用说明 

* [M5Stack Tab5 docs](https://docs.m5stack.com/zh_CN/core/Tab5)

## 快速体验

到 [M5Burner](https://docs.m5stack.com/zh_CN/uiflow/m5burner/intro) 选择 Tab5 搜索小智下载固件

## 基础使用

* idf version: v6.0-dev

1. 调整 idf_component.yml

将
```yaml
  espressif/esp_video:
    version: ==1.3.1   # for compatibility. update version may need to modify this project code.
    rules:
    - if: target not in [esp32]
```
修改为
```yaml
  espressif/esp_video:
    version: '==0.7.0'
    rules:
    - if: target not in [esp32]
  espressif/esp_ipa: '==0.1.0'
```

2. 使用 release.py 编译

```shell
python ./scripts/release.py m5stack-tab5
```

如需手动编译，请参考 `m5stack-tab5/config.json` 修改 menuconfig 对应选项。

3. 编译烧录程序

```shell
idf.py flash monitor
```

> [!NOTE]
> 进入下载模式：长按复位按键（约 2 秒），直至内部绿色 LED 指示灯开始快速闪烁，松开按键。


## log

@2025/05/17 测试问题

1. listening... 需要等几秒才能获取语音输入???
2. 亮度调节不对
3. 音量调节不对
 
## TODO
