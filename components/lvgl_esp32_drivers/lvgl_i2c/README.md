# I2C in `lvgl_esp32_drivers`


&nbsp;


## Information for users

### I2C Manager support

`lvgl_esp32_drivers` integrates [I2C Manager](https://github.com/ropg/i2c_manager), which is used in case you select a touch sensor or screen that uses the I2C bus. If you're just using LVGL you don't need to do anything special.

I2C Manager can help if you are in a situation where you want to avoid "bus conflicts" on the I2C bus. Suppose you use LVGL with a touch sensor that uses I2C, and your device also has another I2C device that needs to be read frequently, such as a 3D-accelerometer. ESP-IDF is not inherently "thread-safe". So if you read that from another task than the one LVGL uses to read the touch data, you need some kind of mechanism to keep these communications from interfering. 

If you have (or write) a driver for that 3D-accelerometer that can use I2C Manager (or the I2C HAL and i2cdev abstraction layers that I2C Manager is compatible with) then put I2C Manager in your components directory by cloning the repository from below and in your main program do:

```c
#include "i2c_manager.h"
#include "lvgl_helpers.h"

[...]

lvgl_i2c_locking(i2c_manager_locking());
lv_init();
lvgl_driver_init();
```

The `lvgl_i2c_locking` part will cause the LVGL I2C driver to play nice with anything else that uses the I2C port(s) through I2C Manager.


&nbsp;


## Information for LVGL driver developers

I2C support in the LVGL ESP drivers is provided exclusively by the files in this directory. Driver code that uses I2C communicates through the functions provided in `i2c_manager.h`. 


&nbsp;


### Using I2C in an LVGL driver, a multi-step guide

<dl>

<dt>Step 1</dt>

<dd>
The Kconfig entries for your driver only need to specify that you will be using I2C. This is done by adding `select LV_I2C_DISPLAY` or `select LV_I2C_TOUCH`.
</dd>


<dt>Step 2</dt>

<dd>
To use the I2C port in your code you would do something like:

    
```c
#include "lvgl_i2c/i2c_manager.h"

uint8_t data[2];
lvgl_i2c_read(CONFIG_LV_I2C_TOUCH_PORT, 0x23, 0x42, &data, 2);
```

This causes a touch driver to read two bytes at register `0x42` from the IC at address `0x23`. Replace `CONFIG_LV_I2C_TOUCH_PORT` by `CONFIG_LV_I2C_DISPLAY_PORT` when this is a display instead of a touch driver. `lvgl_i2c_write` works much the same way, except it writes the bytes from the buffer instead of reading them. _(It's ignored above but these functions return `esp_err_t` so you can check if the I2C communication worked.)_
</dd>

<dt>Step 3</dt>

<dd>
There is no step 3, you are already done.
</dd>

</dl>


### Meanwhile, behind the scenes ...

If any of the drivers selected by the user uses I2C, the menuconfig system will show an extra menu to select I2C port(s) for screen and/or touch sensor. An additional menu allows for setting of GPIO pins and bus speed of any port selected for use with LVGL. It's perfectly fine for a display and a touch sensor to be on the same I2C port or different ones. 


&nbsp;


## More information

If you need more documentation, please refer to the [I2C Manager GitHub repository](https://github.com/ropg/i2c_manager) for more detailed information on how I2C manager works. There are features not in the simple example above, such as reads and writes without specifying a register, 16-bit registers, 10-bit I2C addressing and more. 
