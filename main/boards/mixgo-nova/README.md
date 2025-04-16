# Mixgo_Nova(元控·青春) 开发板

<img src="https://mixly.cn/public/icon/2024/6/09705006c1c643beb96338791ee1dea0_m.png" alt="Mixgo_Nova" width="200"/>

&zwnj;**[Mixgo_Nova](https://mixly.cn/fredqian/mixgo_nova)**&zwnj; 是一款专为物联网、教育及创客项目设计的多功能开发板，集成丰富传感器与无线通信模块，支持图形化编程（Mixly）和离线语音交互，适合快速原型开发与教学。

---

## 🛠️  编译配置命令

**ES8374 CODE MIC采集问题：**

```
managed_components\espressif__esp_codec_dev\device\es8374

static int es8374_config_adc_input(audio_codec_es8374_t *codec, es_adc_input_t input)
{
    int ret = 0;
    int reg = 0;
    ret |= es8374_read_reg(codec, 0x21, &reg);
    if (ret == 0) {
        reg = (reg & 0xcf) | 0x24;
        ret |= es8374_write_reg(codec, 0x21, reg);
    }
    return ret;
}

PS: L386 reg = (reg & 0xcf) | 0x14; 改成 reg = (reg & 0xcf) | 0x24;
```

**配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> 元控·青春
```

**修改 psram 配置：**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> QUAD Mode PSRAM
```

**修改 Flash 配置：**

```
Serial flasher config -> Flash size -> 8 MB
Partition Table -> Custom partition CSV file -> partitions_8M.csv
```

**编译：**

```bash
idf.py build
```

**合并BIN：**

```bash
idf.py merge-bin -o xiaozhi-nova.bin -f raw
```