# Waveshare ESP32-S3-Touch-LCD-3.49 V2

产品链接：https://www.waveshare.net/shop/ESP32-S3-Touch-LCD-3.49.htm

V2 与原版共用屏幕、触摸和音频逻辑，仅板级接线不同：

- 背光 PWM 从 GPIO8 调整为 GPIO42。
- GPIO8 用于 TCA9554 的 EXIO_INT。
- LCD 复位由 TCA9554 EXIO5 控制。
- 背光使能由 TCA9554 EXIO1 控制。
