# Apply Custom Format In Video

(See the [README.md](../README.md) file in the upper level [examples](../) directory for more information about examples.)

This example demonstrates how to initialize the video system using a custom format description. There are three steps to implement this feature:

1. The camera sensor can only work properly when the developer provides the correct register configuration. Therefore, the correct initializer list needs to be provided:

   ```c
   const sc2336_reginfo_t init_reglist_custom_MIPI_2lane_800x800_raw8_30fps[] = {
       {0x0103, 0x01},
       {0x0100, 0x00}, // sleep en
       ...
   };
   ```

2. To use a custom register configuration, the corresponding description information needs to be provided so that the system can get the correct initialization parameters:

   ```c
   const esp_cam_sensor_format_t custom_format_info = {
       .name = "MIPI_2lane_24Minput_RAW8_800x800_30fps",
       .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
       .port = ESP_CAM_SENSOR_MIPI_CSI,
       .xclk = 24000000,
       .width = 800,
       .height = 800,
       .regs = init_reglist_custom_MIPI_2lane_800x800_raw8_30fps,
       .regs_size = ARRAY_SIZE(init_reglist_custom_MIPI_2lane_800x800_raw8_30fps),
       .fps = 30,
       .isp_info = &custom_fmt_isp_info,
       .mipi_info = {
           .mipi_clk = 336000000,
           .lane_num = 2,
           .line_sync_en = false,
       },
       .reserved = NULL,
   };
   ```

   Note that if the camera sensor does not need to use the ISP module provided by the development board, then there is no need to provide ISP-related data.

3. The `ioctl()` is used to set a custom format for the sensor:

   ```c
   if (ioctl(fd, VIDIOC_S_SENSOR_FMT, &custom_format_info) != 0) {
   	ret = ESP_FAIL;
   	goto exit_0;
   }
   ```

​	Note that after executing the `VIDIOC_STREAMON` command, reconfiguring the output format is not allowed.

## How to use example

### Configure the Project

Configure camera hardware data interface based on development kit:

#### MIPI-CSI Development Kit

```
Example Configuration  --->
    Camera sensor interface (MIPI-CSI)  --->
        (X) MIPI-CSI
    (0) MIPI CSI SCCB I2C Port Number
    (8) MIPI CSI SCCB I2C SCL Pin
    (7) MIPI CSI SCCB I2C SDA Pin

Component config  --->
    Espressif Camera Sensors Configurations  --->
        [ ] OV2640  --->
        [*] SC2336  ----
```

#### DVP Development Kit

```
Example Configuration  --->
    Camera sensor interface (DVP)  --->
        (X) DVP
    (1) DVP SCCB I2C Port Number (NEW)
    (33) DVP SCCB I2C SCL Pin (NEW)
    (32) DVP SCCB I2C SDA Pin (NEW)

Component config  --->
    Espressif Camera Sensors Configurations  --->
        [*] OV2640  --->
        [ ] SC2336  ----
```
Note: For custom development boards, please update the I2C pins configuration in the `Example Configuration` menu.

### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

Running this example, you will see the following log output on the serial monitor:

#### MIPI-CSI Development Kit

```
...
I (1627) main_task: Calling app_main()
I (1627) gpio: GPIO[22]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (1637) gpio: GPIO[23]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (1647) sc2336: Detected Camera sensor PID=0xcb3a
I (1727) example: version: 0.5.1
I (1727) example: driver:  MIPI-CSI
I (1727) example: card:    MIPI-CSI
I (1727) example: bus:     esp32p4:MIPI-CSI
I (1727) example: capabilities:
I (1737) example:       VIDEO_CAPTURE
I (1737) example:       STREAMING
I (1747) example: device capabilities:
I (1747) example:       VIDEO_CAPTURE
I (1747) example:       STREAMING
I (1827) example: Capture  format frames for 3 seconds:
I (4847) example:       width:  800
I (4847) example:       height: 800
I (4847) example:       size:   1280000
I (4857) example:       FPS:    30
I (4857) main_task: Returned from app_main()
```