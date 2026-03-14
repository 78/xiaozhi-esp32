## Wireless-Tag WTP4C5MP07S

[Wireless-Tag WTP4C5MP07S](https://shop.wireless-tag.com/products/7inch-lcd-touch-screen-1024x600-mipi-smart-displays-wtp4c5mp07s-esp32-lcd-board-used-with-esp32-p4-and-esp32-c5-dev-board) product is a combo of
* [Wireless-Tag WT99P4C5-S1](https://en.wireless-tag.com/product-item-66.html) ESP32-P4 development board and
* 7 inch 1024x600 ZX7D00C1060M002A MIPI DSI LCD display

<br>

## Configuration

### ESP32P4 Configuration

* Set the compilation target to ESP32P4

        idf.py set-target esp32p4

* Open menuconfig

        idf.py menuconfig

* Select the board

        Xiaozhi Assistant -> Board Type -> Wireless-Tag WTP4C5MP07S

* Select PSRAM

        Component config -> ESP PSRAM -> PSRAM config -> Try to allocate memories of WiFi and LWIP in SPIRAM firstly -> No

* Select Wi-Fi slave target

        Component config -> Wi-Fi Remote -> choose slave target -> esp32c5

* Select Wi-Fi buffers

        Component config -> Wi-Fi Remote -> Wi-Fi configuration -> Max number of WiFi static RX buffers -> 10
        Component config -> Wi-Fi Remote -> Wi-Fi configuration -> Max number of WiFi dynamic RX buffers -> 24
        Component config -> Wi-Fi Remote -> Wi-Fi configuration -> Max number of WiFi static TX buffers -> 10

* Build

        idf.py build

### ESP32C5 Configuration

* Flash the slave example from the esp-hosted-mcu library for the target chip ESP32C5. The esp-hosted-mcu version must match the one used in the xiaozhi-esp32 library.
