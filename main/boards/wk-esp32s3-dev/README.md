# WK ESP32-S3 Dev Board（维控智能 ESP32-S3 开发板）

WK 是维控智能的开发板品牌/板卡系列名称。WK ESP32-S3 Dev Board 是一款基于 ESP32-S3 的通用开发板，面向智能语音、AI 交互和物联网应用场景设计。

该开发板集成麦克风、音频功放、显示屏接口以及常用外设接口，并引出了大部分 GPIO，方便用户连接传感器、执行器和其他功能模块，用于语音交互、屏幕显示、联网控制等功能测试。

## 产品链接

[维控智能 WK ESP32-S3 Dev Board](https://item.taobao.com/item.htm?id=922489696592)

## Hardware

- 平台: ESP32-S3N16R8
- 显示屏: 支持常用 LCD 和 OLED 显示屏
- 音频：板载麦克风和功放
- 板子所在目录: `main/boards/wk-esp32s3-dev`


# 编译配置命令


**进入工程**

```bash
cd xiaozhi-esp32
```

**配置编译目标为 ESP32S3**

```bash
idf.py set-target esp32S3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> 维控智能开发板
```

**编译：**

```bash
idf.py build
```

## Lila build

This fork ships a Lila configuration for this board (Spanish UI, `lila` wake
word, ST7789 240x240 panel, optional web search and Telegram tools). Do not edit
sources or `sdkconfig` for it; describe the build in `.env` and run the wrapper:

```bash
cp .env.example .env          # then edit the values you need
python scripts/build_lila.py  # add --dry-run to only print the command
```

`.env` keys, all optional:

| Key | Default | Meaning |
| --- | --- | --- |
| `LILA_BOARD` | `wk-esp32s3-dev` | board directory under `main/boards` |
| `LILA_NAME` | `wk-esp32s3-dev` | build variant name (also the OTA board identity) |
| `LILA_LANGUAGE` | `es-ES` | display/voice locale |
| `LILA_WAKE_WORD` | `custom:lila` | WakeNet model, `custom:<word>` or `disabled` |
| `LILA_DISPLAY` | `LCD_ST7789_240X240_7PIN` | panel symbol, see below |
| `LILA_TAVILY_API_KEY` | *(empty)* | enables the `web.search` tool |
| `LILA_TELEGRAM_BOT_TOKEN` | *(empty)* | enables the `self.telegram.send` tool |
| `LILA_TELEGRAM_CHAT_ID` | *(empty)* | destination chat for that tool |
| `LILA_SDKCONFIG` | *(empty)* | extra `CONFIG_KEY=VALUE` overrides, applied last |
| `LILA_ZIP` | `0` | `1` also packages `releases/*.zip` |

`LILA_DISPLAY` selects the board's display type and the panel driver:

- `LCD_ST7789_240X240_7PIN` — 7-pin module without CS (GND, VCC, SCL, SDA, RES,
  DC, BLK), SPI mode 3, the most common panel for this build.
- `LCD_ST7789_240X240` — module wired with a dedicated CS line, SPI mode 0.
- `OLED_*` — any OLED symbol from menuconfig, instead of the LCD.

The wrapper only forwards to `scripts/build.py`, which gains two generic
options used by it: `--wake-word custom:<word>` (a MultiNet custom command, so
the word is packed into the assets partition by
`scripts/build_default_assets.py`) and a repeatable `--sdkconfig KEY=VALUE`
override applied after every derived option, for example:

```bash
python scripts/build.py wk-esp32s3-dev --name wk-esp32s3-dev \
  --language es-ES --wake-word custom:lila \
  --sdkconfig CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=15
```

### Optional MCP tools

Both tools are compiled in only when their Kconfig strings are non-empty, so a
plain build exposes neither:

- `web.search` (`query`, `max_results` 1-10) queries Tavily and returns the
  answer plus numbered results with truncated snippets.
- `self.telegram.send` (`text`) sends a Telegram message from a one-shot worker
  task, so the main task never blocks on the network.

The same values can be set for any board in `idf.py menuconfig` under
`Xiaozhi Assistant` → `Tavily API key`, `Telegram bot token`,
`Telegram chat ID`.

