此开发板硬件版本为1.4，如果硬件版本为1.5的话修改调整一下io口
可以查看官方文档，确认具体细节https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html
在config.h和pin_config.h中更改以下的io即可
具体调整为：
I2C_SCL     IO18    ->     IO48
I2C_SDA     IO8     ->     IO47
LCD_DATA6   IO47    ->     IO8
LCD_DATA7   IO48    ->     IO18



本版本只支持了480x480的屏幕