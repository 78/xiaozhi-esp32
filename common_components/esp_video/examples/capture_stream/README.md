# Capture Stream Example

(See the [README.md](../README.md) file in the upper level [examples](../) directory for more information about examples.)

This example demonstrates the following:

- How to initialize esp_video with specific parameters
- How to open video device and capture data stream from this device

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
I (1075) main_task: Calling app_main()
I (1085) gpio: GPIO[31]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (1095) gpio: GPIO[34]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (1105) sc2336: Detected Camera sensor PID=0xcb3a with index 0
I (1175) example: version: 0.1.0
I (1175) example: driver:  MIPI-CSI
I (1175) example: card:    MIPI-CSI
I (1175) example: bus:     esp32p4:MIPI-CSI
I (1185) example: capabilities:
I (1185) example:       VIDEO_CAPTURE
I (1195) example:       READWRITE
I (1195) example:       STREAMING
I (1195) example: device capabilities:
I (1205) example:       VIDEO_CAPTURE
I (1205) example:       READWRITE
I (1215) example:       STREAMING
I (1215) example: Capture RAW8 BGGR format frames for 3 seconds:
I (4235) example:       width:  1280
I (4235) example:       height: 720
I (4235) example:       size:   921600
I (4245) example:       FPS:    30
I (4245) example: Capture RGB 5-6-5 format frames for 3 seconds:
I (7265) example:       width:  1280
I (7265) example:       height: 720
I (7265) example:       size:   1843200
I (7265) example:       FPS:    30
I (7275) example: Capture RGB 8-8-8 format frames for 3 seconds:
I (10295) example:      width:  1280
I (10295) example:      height: 720
I (10295) example:      size:   2764800
I (10295) example:      FPS:    30
I (10305) example: Capture YUV 4:2:0 format frames for 3 seconds:
I (13325) example:      width:  1280
I (13325) example:      height: 720
I (13325) example:      size:   1382400
I (13325) example:      FPS:    30
I (13325) example: Capture YVU 4:2:2 planar format frames for 3 seconds:
I (16355) example:      width:  1280
I (16355) example:      height: 720
I (16355) example:      size:   1843200
I (16355) example:      FPS:    30
I (16355) main_task: Returned from app_main()
...
```

#### DVP Development Kit

```
...
I (867) main_task: Calling app_main()
I (867) gpio: GPIO[32]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (867) gpio: GPIO[33]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (877) ov2640: Detected Camera sensor PID=0x26 with index 0
I (957) example: version: 0.3.0
I (957) example: driver:  DVP
I (957) example: card:    DVP
I (957) example: bus:     esp32p4:DVP
I (957) example: capabilities:
I (957) example:        VIDEO_CAPTURE
I (957) example:        READWRITE
I (957) example:        STREAMING
I (957) example: device capabilities:
I (957) example:        VIDEO_CAPTURE
I (957) example:        READWRITE
I (957) example:        STREAMING
I (957) example: Capture RGB 5-6-5 format frames for 3 seconds:
I (4367) example:       width:  640
I (4367) example:       height: 480
I (4367) example:       size:   614400
I (4367) example:       FPS:    6
I (4367) main_task: Returned from app_main()
...
```