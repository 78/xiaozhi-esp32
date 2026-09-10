# Waveshare ESP32-C6-ePaper-1.54

## Pin Map

This board definition follows the ESP32-C6 pin table provided for the
Waveshare ESP32-C6-ePaper-1.54 hardware:

- EPD: SDI GP5, SCLK GP6, CS GP7, BUSY GP10, RST GP11, D/C GP15
- I2C: SDA GP18, SCL GP8
- Audio I2S: MCLK GP19, DOUT GP20, SCLK GP21, LRCK GP22, DIN GP23
- Buttons: BOOT0 GP9, BAT_KEY GP2
- Battery ADC: GP0
- IO expander: INT GP1, EPD power EXIO0, audio power EXIO1, PA control EXIO3,
  green LED EXIO4, battery control EXIO5, EPD touch INT EXIO6

## Build

This ESP32-C6 board has no PSRAM and uses 16MB flash. It uses the 16MB C3/C6
partition layout with a 4000K assets partition because C6 cannot mmap the
standard 8MB assets partition.

```bash
python ./scripts/build.py waveshare/esp32-c6-epaper-1.54 --name esp32-c6-epaper-1.54
```
