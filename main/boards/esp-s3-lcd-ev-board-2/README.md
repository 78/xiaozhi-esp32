请确认自己的开发板硬件版本，如果硬件版本，在配置中进行ev_board type进行选择
1.4与1.5只有io进行变更
可以查看官方文档，确认具体细节https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html
具体调整为：
I2C_SCL     IO18    ->     IO48
I2C_SDA     IO8     ->     IO47
LCD_DATA6   IO47    ->     IO8
LCD_DATA7   IO48    ->     IO18

本版本只支持了800x480的屏幕