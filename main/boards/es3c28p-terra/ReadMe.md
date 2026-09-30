# ES3C28P (LCDWiki 2.8" ESP32-S3) - Terra

Derived from `freenove-esp32s3-display-2.8-lcd`. Changes:
- I2S data pins swapped: mic DIN=GPIO8, speaker DOUT=GPIO6 (per LCDWiki pin table and the ES3C28P BSP).
- OTA URL: https://terra.terranodex.com/xiaozhi/ota/

Unverified on real hardware (check after first flash):
- Display `DISPLAY_INVERT_COLOR` / `BGR` order (copied from Freenove). If colors look inverted or red/blue swapped, change them in config.h.
- PA (GPIO1) is treated as active LOW (pa_inverted=true in the .cc, as in Freenove).
