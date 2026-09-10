## LILYGO T-CameraPlus-S3

T-CameraPlus-S3 is an intelligent camera module developed based on the ESP32S3 chip, equipped with a 240x240 TFT display, digital microphone, speaker, independent button, power control chip, SD card module, etc. It comes with a basic UI written based on LVGL, which can achieve functions such as file management, music playback, recording, and camera projection (if the factory does not write the program, you need to manually burn the UI program named "Lvgl_UI").

Official github: [T-CameraPlus-S3](https://github.com/Xinyuan-LilyGO/T-CameraPlus-S3)

## Configuration

**Set the compilation target to ESP32S3**

```bash
idf.py set-target esp32s3
```

**Open menuconfig**

```bash
idf.py menuconfig
```

**Select the board**

```
Xiaozhi Assistant -> Board Type -> LILYGO T-CameraPlus-S3_V1_0_V1_1
Or
Xiaozhi Assistant -> Board Type -> LILYGO T-CameraPlus-S3_V1_2
```

**Modify the psram configuration**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Quad Mode PSRAM
```


**Select and set camera sensor**

```
Component config -> Espressif Camera Sensors Configurations -> Camera Sensor Configuration -> Select and Set Camera Sensor -> OV2640 -> Select default output format for DVP interface -> YUV422 240x240 25fps, DVP 8-bit, 20M input
```


**Build**

```bash
idf.py build
```