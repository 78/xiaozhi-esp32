# RYMCU BigSmart

该目录为 `RYMCU BigSmart` 开发板适配，并按以下硬件资源完成映射：

- 主控：ESP32-S3-WROOM-1-N16R8
- 显示：ST7789（320x240，SPI）
- 触摸：GT911（I2C）
- 音频：ES8311 + ES7210（I2S + I2C）
- IO扩展：PCA9557（I2C 地址 `0x19`）
- 摄像头：GC0308（DVP）

参考硬件文档：

- https://github.com/rymcu/BigSmart-Open/blob/main/docs/rymcu-bigsmart-hardware.md

## 编译

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

在菜单中选择：

`Xiaozhi Assistant -> Board Type -> RYMCU BigSmart`

然后执行：

```bash
idf.py build
```
