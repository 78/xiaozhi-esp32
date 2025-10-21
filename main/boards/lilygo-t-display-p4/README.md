## LILYGO T-Display-P4

The T-Display-P4 is a versatile development board based on the ESP32-P4 core. Its features include:  

1. **High Processing Power**: Equipped with the high-performance ESP32-P4 core processor, it can handle more complex graphics and video tasks, delivering smoother display performance.  
2. **Low Power Design**: Offers multiple selectable power modes to effectively reduce energy consumption and extend battery life.  
3. **High-Resolution Display**: Supports high resolution (default with a large MIPI interface screen at 540x1168px), providing sharp and clear visuals.  
4. **Rich Peripheral Support**: Onboard peripherals include an HD MIPI touchscreen, ESP32-C6 module, speaker, microphone, LoRa module, GPS module, Ethernet, a linear vibration motor, an independent battery gauge for monitoring battery health and percentage, and an MIPI camera. Multiple GPIOs of both the ESP32-P4 and ESP32-C6 are exposed, enhancing the device's expandability.  

Official github: [T-Display-P4](https://github.com/Xinyuan-LilyGO/T-Display-P4)

## Configuration

### ESP32P4 Configuration

* Set the compilation target to ESP32P4

        idf.py set-target esp32p4

* Open menuconfig

        idf.py menuconfig

* Select the board

        Xiaozhi Assistant -> Board Type -> LILYGO T-Display-P4

* Select the screen type

        Xiaozhi Assistant -> Select the screen type -> (Select the screen type you need to configure)

* Screen the screen pixel format

        Xiaozhi Assistant -> Select the color format of the screen -> (Select the screen pixel format you need to configure)

* Build

        idf.py build

### ESP32C6 Configuration

* Flash the slave example from the esp-hosted-mcu library for the target chip ESP32C6. The esp-hosted-mcu version must match the one used in the xiaozhi-esp32 library.