# Espressif Camera Sensors Component

This component provides drivers for camera sensors that can be used on the ESP32 series chips.  

It is highly recommended that users use it in the [esp-video](https://github.com/espressif/esp-video-components/tree/master/esp_video) component.

[![Component Registry](https://components.espressif.com/components/espressif/esp_cam_sensor/badge.svg)](https://components.espressif.com/components/espressif/esp_cam_sensor)

## Supported Camera Sensors

| model   | max resolution | output interface | output format                                                | Len Size |
| ------- | -------------- | ---------- | ------------------------------------------------------------ | -------- |
| SC2336  | 1920 x 1080    | MIPI & DVP      | 8/10-bit Raw RGB data | 1/3"     |
| SC202CS(SC2356) | 1600 x 1200    | MIPI      | 8/10-bit Raw RGB data | 1/5.1"     |
| OV5645  | 2592 x 1944    | MIPI      | 8/10-bit Raw RGB data<br/>RGB565<br/>YUV/YCbCr422<br/>YUV420 | 1/4"     |
| OV5647  | 2592 x 1944    | MIPI & DVP      | 8/10-bit Raw RGB data | 1/4"     |
| OV2640  | 1600 x 1200    | DVP | 8/10-bit Raw RGB data<br/>JPEG compression<br/>YUV/YCbCr422<br/>RGB565 | 1/4"     |
| OV2710  | 1920 x 1080    | MIPI | Raw RGB data | 1/2.7"     |
| GC0308  | 640 x 480    | DVP | Grayscale<br/>YCbCr422<br/>RGB565 | 1/6.5"     |
| GC2145  | 1600 x 1200    | MIPI & DVP | RGB565<br/>YCbCr422<br/>8bit Raw RGB data | 1/5"     |
| SC101IOT  | 1280 x 720    | DVP | YCbCr422<br/>8/10-bit Raw RGB data | 1/4.2"     |
| SC030IOT  | 640 x 480    | DVP | YCbCr422<br/>8bit Raw RGB data | 1/6.5"     |
| BF3925  | 1600 x 1200    | DVP | YCbCr422<br/>8bit Raw RGB data | 1/5"     |
| SC035HGS  | 640 x 480    | MIPI & DVP | Raw MONO<br/>Raw RGB data | 1/6"     |

## Steps to add a new camera sensor driver

### Add a directory to store sensor driver code

The best way to add a camera driver is to copy an existing sensor driver directory and rename its file names and function names. 

Below is a brief description of the files in the driver folder.

```
├── cfg
│   ├── sc2336_default.json         Configuration of the IPA(Image processing algorithms)
├── include
│   └── sc2336_types.h
│   └── sc2336.h
├── private_include
│   └── sc2336_regs.h
│   └── sc2336_settings.h           Initialize settings of the sensor driver
└── Kconfig.sc2336
└── sc2336.c                        Implementation of the sensor driver
```

According to the type of your camera sensor, select a driver of the same type as a reference.

There are two types of sensors according to the data format they output.

- YUV sensor. Such sensors can output image data in YUV format directly. Usually it has an ISP module inside, so the output data can be used. It does not require a JSON configuration file in the cfg directory.
- RAW sensor. This type of sensor usually only outputs data in RAW format. The RAW format data needs to be imported into the ISP module of the baseboard for optimization.

For RAW format sensors, image quality tuning is typically required, as described below.

- New sensor driver docking
- Sensor and lens calibration(BLC, DPC, NP, LSC,AWB,CCM...)
- Brightness and color tuning(AE, AWB, Gamma, CCM, Saturation...)
- Transparency tuning(Gamma, Contrast...)
- Sharpness tuning(DPC, BNR, Demosaic, Sharpen...)

Therefore, for RAW sensor, it requires a lot of tuning. Please understand this before adding drivers. It is recommended to use the camera sensor that has been adapted.

### Get the sensor initialize settings

Request initial configuration from the FAE of the company selling the sensor. Then, write the initialization data into the `sensor_settings.h` file.

```c
static const sc2336_reginfo_t init_reglist_MIPI_2lane_720p_25fps[] = {
    {0x0103, 0x01},
    {SC2336_REG_SLEEP_MODE, 0x00},
    ...
};
```

Note that this configuration file comes from the technicians who sell the sensor. Platform developers cannot create it based on the datasheet.

Add the description information of the initialization data in `sensor.c`. This descriptive information is used to initialize the modules on the baseboard.

```c
static const esp_cam_sensor_format_t sc2336_format_info[] = {
    /* For MIPI */
    {
        .name = "MIPI_2lane_24Minput_RAW10_1280x720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[0],
        .mipi_info = {
            .mipi_clk = 405000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
};
```

### Implement key functions

Some functions are called automatically, so they must be implemented.

- sensor_detect(). Detect whether the sensor is connected.
- sensor_set_format(). Write initialization data to the sensor.
- sensor_set_stream(). Control the start and stop of image data stream.
- sensor_query_para_desc(). Query parameter details.
- sc2336_set_para_value(). Set parameter value.

```c
static const esp_cam_sensor_ops_t sc2336_ops = {
    .query_para_desc = sc2336_query_para_desc,
    .get_para_value = sc2336_get_para_value,
    .set_para_value = sc2336_set_para_value,
    .query_support_formats = sc2336_query_support_formats,
    .query_support_capability = sc2336_query_support_capability,
    .set_format = sc2336_set_format,
    .get_format = sc2336_get_format,
    .priv_ioctl = sc2336_priv_ioctl,
    .del = sc2336_delete
};
esp_cam_sensor_device_t *sc2336_detect(esp_cam_sensor_config_t *config)
{
	esp_cam_sensor_device_t *dev = NULL;
	...
}
```

For RAW sensors, it is also necessary to implement their gain control.

```c
static const uint32_t sc2336_total_gain_val_map[] = {
};
static const sc2336_gain_t sc2336_gain_map[] = {
};
```

According to the data interface of this module, add a macro definition for automatic detection.

```c
// For mipi
#if CONFIG_CAMERA_SC2336_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc2336_detect, ESP_CAM_SENSOR_MIPI_CSI, SC2336_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return sc2336_detect(config);
}
#endif
// For DVP
#if CONFIG_CAMERA_SC2336_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc2336_detect, ESP_CAM_SENSOR_DVP, SC2336_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return sc2336_detect(config);
}
```

### Update compilation files and documentation

Taking SC2336 as an example, the updates of each file are as follows:

- `esp_cam_sensor/CHANGELOG.md`

  ```
  ## x.x.0
  - - Added support for SC2336 MIPI camera sensor driver
  ```

- `esp_cam_sensor/CMakeLists.txt`

  ```cmake
  if(CONFIG_CAMERA_SC2336)
      list(APPEND srcs "sensors/sc2336/sc2336.c")
      list(APPEND include_dirs "sensors/sc2336/include")
      list(APPEND priv_include_dirs "sensors/sc2336/private_include")
  endif()
  
  if(CONFIG_CAMERA_SC2336_AUTO_DETECT)
      target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc2336_detect")
  endif()
  ```

- `esp_cam_sensor/Kconfig`

  ```
  rsource "sensors/sc2336/Kconfig.sc2336"
  ```

- `esp_cam_sensor/idf_component.yml`

  ```
  version: "x.x.0"
  ```

- `esp_cam_sensor/project_include.cmake`

  ```
  if(CONFIG_CAMERA_SC2336)
      if(CONFIG_CAMERA_SC2336_DEFAULT_IPA_JSON_CONFIGURATION_FILE)
          idf_build_set_property(ESP_IPA_JSON_CONFIG_FILE_PATH "${COMPONENT_PATH}/sensors/sc2336/cfg/sc2336_default.json" APPEND)
      elseif(CONFIG_CAMERA_SC2336_CUSTOMIZED_IPA_JSON_CONFIGURATION_FILE)
          idf_build_set_property(ESP_IPA_JSON_CONFIG_FILE_PATH ${CONFIG_CAMERA_SC2336_CUSTOMIZED_IPA_JSON_CONFIGURATION_FILE_PATH} APPEND)
      endif()
  endif()
  ```

  Note that only RAW sensors need to update this file.

- `esp_cam_sensor/README.md`

  ```
  | SC2336  | 1920 x 1080    | MIPI & DVP      | 8/10-bit Raw RGB data | 1/3"     |
  ```

### Add an option for this sensor in the testing program

Add the option to enable this sensor in `esp_cam_sensor/test_apps/detect/sdkconfig.ci.all_cameras`.

```
CONFIG_CAMERA_SC2336=y
```

## Steps to add a custom initialization list to the supported camera sensor

If the initialization settings of the camera sensor need to be updated, follow the steps below to implement it in the application layer code.

- Add an initialization array and its corresponding description information. 
- Call `ioctl(fd, VIDIOC_S_SENSOR_FMT, ...)` write register data into sensor

Refer to the [example](https://github.com/espressif/esp-video-components/tree/master/esp_video/examples/video_custom_format) here.

