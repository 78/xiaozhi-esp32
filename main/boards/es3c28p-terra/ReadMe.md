# ES3C28P (LCDWiki 2.8" ESP32-S3) - Terra

Derived from `freenove-esp32s3-display-2.8-lcd`. Changes:
- OTA URL: https://terra.terranodex.com/xiaozhi/ota/

Audio pins follow the BSP header `include/bsp/esp32_s3_es3c28p.h` of
https://github.com/ngttai/esp32_s3_es3c28p: BSP_I2S_DOUT=GPIO8 (ESP -> codec/speaker),
BSP_I2S_DSIN=GPIO6 (codec/mic -> ESP). These equal the Freenove values.

History: an earlier revision of this board swapped DIN/DOUT (6/8) based on a web-page
summary of the LCDWiki table; on hardware that produced garbled/crackling speaker output.
The BSP source is authoritative.

Unverified on hardware:
- PA (GPIO1): LCDWiki says low-level enable and this board uses pa_inverted=true (works: sound is played).
  The ngttai BSP passes pa_reverted=false; the two sources disagree.
- Display orientation/colour: invert=true and BGR match the BSP; this board uses landscape (swap_xy=true, copied from Freenove).
