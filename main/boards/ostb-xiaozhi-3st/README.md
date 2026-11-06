# OSTB Xiaozhi 3st (Box D-Solution 2.0" 2-Mic)

- **Website:** https://dsolution.vn/
- **Shopee:** https://shopee.vn/dsolution.tb

## Hardware Specifications
- **MCU:** ESP32-S3-WROOM-1 (N16R8, 16MB Flash, 8MB Octal PSRAM)
- **Display:** 2.0" NV3030B / NV3023 LCD (296x240 active area, SPI)
  - Display Width: 296, Height: 240
  - Offset X: 24, Offset Y: 0
  - Swap XY: true, Mirror X: true, Mirror Y: true
- **Audio:** ES8311 (DAC/Amp) + ES7210 (ADC Mic Array, TDM Slave)
- **Touchscreen:** CST816S (I2C)
- **Power:** Onboard LiPo battery charger, battery ADC voltage monitoring
- **Buttons:**
  - BOOT / Wakeup: GPIO 0 (Single click: Toggle chat, 6-click: Reset WiFi AP)
  - Volume Up: GPIO 40 (Single click: Volume +, Long press: Max volume, Double click: Toggle chat)
  - Volume Down: GPIO 39 (Single click: Volume -, Long press: Mute, 5-click: Reset WiFi AP)

## Documentation
- [PINOUT.md](PINOUT.md) — Pinout, display, audio, and hardware mapping.
- [PARTITIONS.md](PARTITIONS.md) — 16MB Flash memory map and partition profile (`partitions/v2/16m_ostb_xiaozhi_3st.csv`).
