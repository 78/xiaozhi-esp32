# ESP-Video Examples

## Hardware Required
* A ESP32-P4 development board with camera interface (e.g., ESP32-P4-Function-EV-Board).
* A camera sensor that has been supported, see the [esp_cam_sensor](https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor).
* A USB Type-C cable for power supply and programming.

## Enable ISP Pipelines
For sensors that output data in RAW format, the ISP controller needs to be enabled to improve image quality.
```
Component config  --->
    Espressif Video Configuration  --->
        Enable ISP based Video Device  --->
            [*] Enable ISP Pipeline Controller
```