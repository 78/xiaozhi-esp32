# RockBase IoT NM-EPD-420 (BW)

XiaoZhi AI voice assistant port for the [RockBase IoT NM-EPD-420](https://github.com/RockBase-iot/NM-EPD-420)
ESP32-S3 e-paper development board, targeting the **NM-EPD-420-BW** variant with the
GYE042A87 4.2" 400x300 black/white panel (SSD1683 controller, partial refresh ~1 s).

## Hardware used

| Block | Part | Pins |
| --- | --- | --- |
| Display | GYE042A87 400x300 B/W e-paper | FSPI: SCK=2, MOSI=1, CS=46, DC=4, RST=5, BUSY=6 |
| Audio codec | ES8311 (I2C 0x18) | MCLK=21, BCLK=15, WS=17, DOUT=18, DIN=16; PA enable=41; codec power gate=44 |
| Microphone | LMD4737 PDM DMIC via ES8311 DMIC input | shares the I2S BCLK net |
| Buttons | USER / BOOT (active LOW) | USER=45, BOOT=0 |
| Battery | ADC via 2:1 divider behind gate | ADC=GPIO3 (ADC1_CH2), gate=43 |
| Sensor | AHT20 temperature/humidity (I2C 0x38) | SDA=39, SCL=38, power gate=40 |
| LoRa / SD | SX1262 / µSD on HSPI | unused, kept powered off (LORA_EN=47 LOW) |

## Build

```bash
python3 scripts/build.py rockbase-iot/nm-epd-420 --name nm-epd-420-bw
```

## Notes

- **Digital microphone**: the LMD4737 PDM mic enters through the ES8311 DMIC input.
  This firmware passes `digital_mic=true` to `Es8311AudioCodec`, which sets register
  0x14 bits 7:6 (DMIC enable + polarity) after every codec open, matching the
  factory-test-proven configuration.
- **Mic clock margin**: the PDM mic clock shares the I2S BCLK net. At the standard
  24 kHz / 16-bit stereo configuration BCLK = 768 kHz, slightly below the LMD4737
  1.0-3.5 MHz specification (the factory test firmware uses 32 kHz for this reason,
  which Opus does not support). If recordings are noisy on real hardware, switching
  to 32-bit I2S slots (BCLK = 1.536 MHz) is the fallback.
- **Display refresh**: LVGL renders into a PSRAM surface; a background task pushes
  partial refreshes (~1 s) to the panel and forces a full refresh after 10 partials
  or when more than 30% of pixels changed, to keep ghosting in check.
- The tri-color (GDEY042Z98) and four-color (GDEY0420F51) panel variants are not
  supported yet.
