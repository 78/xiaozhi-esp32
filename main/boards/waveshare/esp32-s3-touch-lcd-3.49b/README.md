新增 微雪 开发板: ESP32-S3-Touch-LCD-3.49B (SKU 32375)

产品链接：
https://www.waveshare.net/shop/ESP32-S3-Touch-LCD-3.49B.htm

## 与 ESP32-S3-Touch-LCD-3.49 的区别

3.49B (V1.1) 硬件修订版本改动了背光和液晶复位电路：

- 背光 PWM 信号引脚由 GPIO8 改为 GPIO42。
- 背光电源由 TCA9554 IO 扩展芯片的 `BL_EN` 引脚 (IO_EXPANDER_PIN_NUM_1) 控制使能，
  必须先使能该引脚，PWM 占空比调节才能生效。
- 液晶复位不再直接使用 GPIO21，而是通过 TCA9554 IO 扩展芯片的
  `LCD_RST` 引脚 (IO_EXPANDER_PIN_NUM_5) 控制。

如果误用 3.49 (非 B) 固件，语音助手仍会正常更新亮度状态，但物理背光不会随之变化，
因为 PWM 信号被发送到了未连接的 GPIO8 而不是实际接线的 GPIO42。
