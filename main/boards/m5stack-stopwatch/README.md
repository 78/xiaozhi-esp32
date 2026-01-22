# M5Stack StopWatch

M5Stack StopWatch 开发板支持。

## 硬件特性

- **显示**: CO5300 AMOLED 圆形显示屏，466x466 分辨率，1.75 英寸
- **音频编解码器**: ES8311
- **功放**: AW8737A (PA 控制通过单线协议)
- **电源管理**: M5Stack PM1 (PMIC)
- **IO扩展**: M5Stack IOE1 (用于 LCD 和音频电源控制)
- **接口**: QSPI 显示接口

## 引脚配置

### I2C (I2C0)
- SCL: GPIO 48
- SDA: GPIO 47

### 音频 I2S
- MCLK: GPIO 18
- BCLK: GPIO 17
- WS (LRCK): GPIO 15
- DOUT: GPIO 21
- DIN: GPIO 16
- PA: GPIO 14 (AW8737A)

### 显示 QSPI
- SCK: GPIO 40
- CS: GPIO 39
- D0: GPIO 41
- D1: GPIO 42
- D2: GPIO 46
- D3: GPIO 45
- TE: GPIO 38 (Tearing Effect) - no use

### 按钮
- Button PWR: PM1 控制
- Button 1: GPIO 2
- Button 2: GPIO 1

## 编译

使用以下命令编译固件：

```bash
python scripts/release.py m5stack-stopwatch
```

## 注意事项

1. StopWatch 使用圆形 AMOLED 显示屏，不需要背光控制
2. PA (AW8737A) 通过单线协议控制，支持 4 种功率模式
3. 显示和音频电源通过 IOE1 扩展器控制
4. 当前实现使用 IOE1 作为 IO 扩展器，如果实际硬件使用 PYIO，可能需要调整代码

## 参考

- 参考实现: `/home/loki/m5stack_dev/project/uwb_position/stopwatch_test`
- 基于 ChainCaptain 板级实现模式

