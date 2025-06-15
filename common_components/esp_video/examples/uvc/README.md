# USB Video Class Example

(See the [README.md](../README.md) file in the upper level [examples](../) directory for more information about examples.)

This example demonstrates the following:

- How to initialize esp_video with specific parameters
- How to open camera interface video device and capture video stream from this device
- How to open H.264 or JPEG video device and encode video stream by this device
- How to initialize USB video class and see video on the PC

## How to use example

### Software Required

* `potplay` APP for PC on Windows OS

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
        [*] SC2336  ---->
            Default format select for MIPI (RAW8 1280x720 30fps, MIPI 2lane 24M input)  --->
                (X) RAW8 1280x720 30fps, MIPI 2lane 24M input

    USB Device UVC  --->
        USB Cam1 Config  --->
             UVC Default Resolution (HD 1280x720)  --->
                (X) HD 1280x720
            (30) Frame Rate (FPS)
            (1280) Cam1 Frame Width
            (720) Cam1 Frame Height
```
Note: For custom development boards, please update the I2C pins configuration in the `Example Configuration` menu.

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
            Default format select (RGB565 640x480 6fps, DVP 8bit 20M input)  --->
                (X) RGB565 640x480 6fps, DVP 8bit 20M input

    USB Device UVC  --->
        USB Cam1 Config  --->
             Default Resolution (VGA 640x480)  --->
                (X) VGA 640x480
            (6) Frame Rate (FPS)
            (640) Cam1 Frame Width
            (480) Cam1 Frame Height
```

####  Select USB video class output video format

##### JPEG

```
component config  --->
    USB Device UVC  --->
        USB Cam1 Config  --->
             Cam1 Format (MJPEG)  --->
                (X) MJPEG
```

##### H.264

```
component config  --->
    USB Device UVC  --->
        USB Cam1 Config  --->
             UVC Cam1 Format (H264)  --->
                (X) H264
```

***Please note that the OV2640 doesn't support H.264 format, it only supports the JPEG format***.

### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py set-target esp32p4

idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

Running this example, you will see the following log output on the serial monitor:

#### MIPI-CSI Development Kit

```
...
I (1641) main_task: Calling app_main()
I (1641) gpio: GPIO[22]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (1651) gpio: GPIO[23]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (1661) sc2336: Detected Camera sensor PID=0xcb3a
I (1741) example: version: 0.5.1
I (1741) example: driver:  MIPI-CSI
I (1741) example: card:    MIPI-CSI
I (1741) example: bus:     esp32p4:MIPI-CSI
I (1741) example: capabilities:
I (1751) example:       VIDEO_CAPTURE
I (1751) example:       STREAMING
I (1761) example: device capabilities:
I (1761) example:       VIDEO_CAPTURE
I (1761) example:       STREAMING
I (1771) example: version: 0.5.1
I (1771) example: driver:  H.264
I (1781) example: card:    H.264
I (1781) example: bus:     esp32p4:H.264
I (1781) example: capabilities:
I (1791) example:       STREAMING
I (1791) example: device capabilities:
I (1801) example:       STREAMING
I (1801) example: Format List
I (1801) example:       Format(1) = H.264
I (1811) example: Frame List
I (1811) example:       Frame(1) = 1280 * 720 @30fps
I (1821) usbd_uvc: UVC Device Start, Version: 1.1.0
I (1821) main_task: Returned from app_main()
I (2031) usbd_uvc: Mount
I (3281) usbd_uvc: Suspend
...
```

#### DVP Development Kit

```
...
I (1164) main_task: Calling app_main()
I (1164) gpio: GPIO[32]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (1174) gpio: GPIO[33]| InputEn: 1| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0 
I (1194) ov2640: Detected Camera sensor PID=0x26
I (1274) example: version: 0.5.1
I (1274) example: driver:  DVP
I (1274) example: card:    DVP
I (1274) example: bus:     esp32p4:DVP
I (1274) example: capabilities:
I (1284) example:       VIDEO_CAPTURE
I (1284) example:       STREAMING
I (1284) example: device capabilities:
I (1294) example:       VIDEO_CAPTURE
I (1294) example:       STREAMING
I (1304) example: version: 0.5.1
I (1304) example: driver:  JPEG
I (1304) example: card:    JPEG
I (1314) example: bus:     esp32p4:JPEG
I (1314) example: capabilities:
I (1314) example:       STREAMING
I (1324) example: device capabilities:
I (1324) example:       STREAMING
I (1334) example: Format List
I (1334) example:       Format(1) = MJPEG
I (1334) example: Frame List
I (1344) example:       Frame(1) = 640 * 480 @6fps
I (1344) usbd_uvc: UVC Device Start, Version: 1.1.0
I (1354) main_task: Returned from app_main()
I (1564) usbd_uvc: Mount
I (2824) usbd_uvc: Suspend
...
```

Open potplay APP and select follow option in menu:

```
PotPlayer -->
    Open -->
        Camera/Other Device
```

You can see following log on the serial monitor and video in potlayer display window:

```
I (192121) usbd_uvc: Resume
I (192411) usbd_uvc: bFrameIndex: 1
I (192411) usbd_uvc: dwFrameInterval: 333333
```

## Troubleshooting

* If the console log shows as follows, it means your ESP32-P4 chip version is v0.0, and it is not supported by default configuration, please configure the right version by menuconfig:

    ```txt
    A fatal error occurred: bootloader/bootloader.bin requires chip revision in range [v0.1 - v0.99] (this chip is revision v0.0). Use --force to flash anyway
    ```

    menuconfig:
    ```
    Component config  --->
        Hardware Settings  --->
            Chip revision  --->
                Minimum Supported ESP32-P4 Revision (Rev v0.1)  --->
                    (X) Rev v0.0
    ```
