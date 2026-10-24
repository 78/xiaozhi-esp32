# Waveshare ESP32-C5-Touch-LCD-2.8

Board support for the ESP32-C5-Touch-LCD-2.8 with 16 MB flash, quad PSRAM,
a 240 x 320 ST7789 SPI LCD, CST3530 touch, an ES8311 codec, and a
CH32V003 I/O expander. Pin assignments, reset timing, amplifier enable,
and backlight PWM follow the factory BSP.

| Function | Connection |
| --- | --- |
| Shared I2C SDA / SCL | GPIO0 / GPIO1 |
| LCD SCLK / MOSI / MISO | GPIO6 / GPIO7 / GPIO8; shared with SD |
| LCD DC / CS | GPIO9 / GPIO10 |
| SD CS | GPIO23; held high while SD is unused |
| LCD reset | CH32V003 EXIO1 |
| Touch | CST3530 at 0x58; INT GPIO5, reset CH32V003 EXIO0 |
| Audio BCLK / WS / DOUT / DIN | GPIO24 / GPIO25 / GPIO26 / GPIO27 |
| Audio MCLK | Not connected; ES8311 uses BCLK |
| Amplifier enable | CH32V003 EXIO3, active high |
| Backlight | CH32V003 PWM, 0-255, non-inverted |
| BOOT | GPIO28, active low |

Build with ESP-IDF 6.0.1 or newer (6.1 recommended):

```sh
python scripts/build.py waveshare/esp32-c5-touch-lcd-2.8 --name esp32-c5-touch-lcd-2.8 --language zh-CN
idf.py flash monitor
```

For manual configuration, select target `esp32c5`, enable quad PSRAM at
80 MHz, and choose `Waveshare ESP32-C5-Touch-LCD-2.8` in
`Xiaozhi Assistant -> Board Type`. Select the language and wake word in
menuconfig when building directly with `idf.py`.

The default Chinese build uses the "你好小智" wake word. BOOT toggles chat;
during startup it enters Wi-Fi provisioning. Touch is registered with LVGL
for the chat interface.

The standard MCP tools `self.audio_speaker.set_volume` and
`self.screen.set_brightness` provide voice control of volume and brightness
from 0 to 100, for example "音量调到 50" and "屏幕亮度调到 80".
The connected service must support device tool calls. Volume and permanent
brightness settings use the existing settings storage.

The CH32V003 must already contain the factory I/O-expander firmware.
This board support does not initialize the factory applications, SD
filesystem, IMU, RTC, or temperature/humidity sensor.
