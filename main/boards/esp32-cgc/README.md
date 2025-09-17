# 主板开源地址：
- V1:[https://oshwhub.com/wdmomo/esp32-xiaozhi-kidpcb](https://oshwhub.com/wdmomo/esp32-xiaozhi-kidpcb)
- V2:[https://oshwhub.com/wdmomo/esp32-xiaozhi-kidpcb_copy](https://oshwhub.com/wdmomo/esp32-xiaozhi-kidpcb_copy)
- 更多介绍：[wdmomo.fun](https://www.wdmomo.fun:81/doc/index.html?file=001_%E8%AE%BE%E8%AE%A1%E9%A1%B9%E7%9B%AE/0001_%E5%B0%8F%E6%99%BAAI/002_ESP32-CGC%E5%BC%80%E5%8F%91%E6%9D%BF%E5%B0%8F%E6%99%BAAI)

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

**编译：**

```bash
idf.py build
```
