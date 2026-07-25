# Freenove ESP32-S3 ESP32 S3 Capacitive Touch Display CYD WiFi BT, 2.8 Inch 240x320 IPS Screen

[product](https://store.freenove.com/products/fnk0104)

Official github: [freenove-esp32s3-display-2.8-lcd](https://github.com/Freenove/Freenove_ESP32_S3_Display)

Likely the same hardware design as [LCD wiki ES3C28P/ES3N28P](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display)

## MhaiBot face (V1)

This board uses a board-local `MhaiBotDisplay` subclass that draws two neutral rounded eyes with randomized blinking (about 2.5–6 s open, 100–180 ms closed) via a single LVGL timer.

- Emotions `neutral` and `robot_2` show the face and hide the stock center emoji.
- All other emotions keep the existing emoji / GIF path.
- Status bar, notifications, subtitle bar, preview image, Wi-Fi provisioning, touch, audio, wake word, and push-to-talk are unchanged.
- After a preview image times out, face vs emoji visibility is restored by the board override of `SetPreviewImage(nullptr)` (the preview timer calls this virtually).
