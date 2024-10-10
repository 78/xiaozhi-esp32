# Is your driver not listed on the supported drivers list?

Consider contributing to this demo project by adding support to your driver.

## Display driver.

To enable LVGL to work with your display you would need to implement from one up to three callback functions (one function for RGB TFT displays, three functions for monochrome display controllers), you can add more functions to enable backlight control, etc.

All display controller code is stored on the `components/lvgl_esp32_drivers/lvgl_tft` directory, see `disp_driver` and `CMakeLists.txt` to add your code into the idf project.

Create a header and source file named after the display controller (in lowercase). On the header file declare the necessary functions, such as:

```c
/* Configure your display */
void x_init(void);

/* LVGL callbacks */
void x_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
/* Only for monochrome displays */
void x_rounder(lv_disp_drv_t *disp_drv, lv_area_t *area);
void x_set_px(lv_disp_drv_t *disp_drv, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa);
```

Where x is the controller name.

For more information on the function callbacks check LVGL documentation: (Display driver)[https://docs.lvgl.io/v7/en/html/porting/display.html#display-driver].

Add your display functions on `disp_driver_init`, `disp_driver_flush`, `disp_driver_rounder` and `disp_driver_set_px` on the `disp_driver.c` file.

## Input device driver.

To enable LVGL to work with your touch controller you would need to implement an initialization function and one function to get the data out from your touch controller.

All touch controller code is stored on the `components/lvgl_esp32_drivers/lvgl_touch` directory.

Create a header and source file named after the display controller (in lowercase). On the header file declare the necessary functions, such as:

```c
/* Configure your display */
void x_init(void);

/* LVGL callbacks */
bool x_read(lv_disp_drv_t *drv, lv_indev_data_t *data);
```

Where x is the controller name.

For more information on the function callbacks check LVGL documentation: (Display driver)[https://docs.lvgl.io/v7/en/html/porting/indev.html].

## Kconfig and Project Configuration

The ESP32 SDK (ESP-IDF) uses [kconfiglib](https://github.com/ulfalizer/Kconfiglib) which is a Python-based extension to the [Kconfig](https://www.kernel.org/doc/Documentation/kbuild/kconfig-language.txt) system which provides a compile-time project configuration mechanism. Using `idf.py menuconfig` will update the file sdkconfig and, during build, provide the file sdkconfig.h.
