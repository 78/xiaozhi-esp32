# Petoi Bittle AI ESP32-C3

This cumulative product branch extends the initial board-only contribution
described below. Its default variant enables expression-only conversation UI,
bounded action sequences, local command parsing and cached Chinese/English
system resources with both WakeNet models. It disables firmware OTA installation
and uses normal INFO logging with dialogue resource sampling off. The separate
`petoi-bittle-ai-c3-resource-debug` variant enables DEBUG resource snapshots;
`petoi-bittle-ai-c3-locale-english-check` is a temporary English diagnostic only.

With product action sequences enabled, a single UART worker owns a two-item
queue. Single-action results report transmission; sequences wait for raw action
tokens before proceeding and stop on timeout without retry. Tokens do not prove
mechanical completion. The tested BiBoard V1.0 needs the matching OpenCat
`260820` patch forwarding `d` as well as `k/m` to its voice UART. Gait values
below 200 are steps; values from 200 are milliseconds. Cloud reply language is
independent of local wake/UI language and is not automatically guaranteed.

The following queue and feature exclusions describe the initial public board
contribution, not the enabled product variants above. Product startup does not
automatically stand the robot.

This board definition supports the Petoi custom ESP32-C3 voice board with
16 MB flash, an ES8311 codec, a 240 x 280 ST7789 display and a NyBoard UART
connection. It has a distinct OTA identity, `petoi-bittle-ai-c3`; do not flash
the stock `lichuang-c3-dev` firmware onto this hardware.

## Connections

| Function | GPIO / setting |
| --- | --- |
| Codec I2C SDA / SCL | 0 / 1 |
| I2S MCLK / BCLK / WS | 13 / 12 / 8 |
| I2S DIN / DOUT | 10 / 7 |
| Codec PA GPIO | None (no direct GPIO enable in this hardware baseline) |
| ST7789 SCK / MOSI / DC / CS | 3 / 4 / 6 / 5 |
| Display | 240 x 280, Y offset 20, SPI mode 0, 20 MHz, inverted colors |
| Backlight | 2, active low |
| BOOT / chat button | 9 |
| NyBoard TX / RX | 21 / 20, UART1, 9600 baud, 8N1 |
| Console / flashing | USB Serial/JTAG, GPIO18/19 reserved for USB |

Use compatible 3.3 V logic and a common ground; connect TX to the NyBoard RX
and RX to the NyBoard TX. Supply the robot from its own suitable power source.
BiBoard at 115200 baud without a prefix is not supported by this variant.

## Build

Activate ESP-IDF `release/v6.1`, then use the canonical build entry point:

```sh
python scripts/build.py petoi/bittle-ai-c3 --name petoi-bittle-ai-c3 --language zh-CN --wake-word nihaoxiaozhi
```

English example:

```sh
python scripts/build.py petoi/bittle-ai-c3 --language en-US --wake-word hiesp
```

The board uses the project's 16 MB flash and v2 partition defaults. Language
and wake words are build options, not properties of the board identity.
Back up existing firmware, partition data and device settings before flashing.
The older Petoi firmware used a different partition layout: do not migrate by
flashing only the application or by installing a cross-layout OTA update.

## Robot MCP tools

`self.robot.send_command` takes the required string `text`, for example
`{"text":"ksit"}`. Each call sends one printable ASCII command (1–128 bytes),
preceded by the exact bytes `58 41 15`, with no added newline. Control bytes,
non-ASCII text and semicolon-separated sequences are rejected. The four-entry
queue rejects new commands when full; no automatic retry is performed.

The tool reports acceptance into the UART queue, **not physical completion**.
`self.robot.get_status` reports queue depth, transmission state and the last
UART transmit error. RX bytes are drained and can be logged at debug level;
they are not interpreted as action-completion acknowledgements. Test one
action at a time and observe the robot before sending the next action.

Multi-action scheduling, speech keyword fallbacks, expression animation and
product-specific language policy are outside this initial board contribution.

## Hardware verification

Build success alone is insufficient. Verify cold boot, display orientation,
backlight, button, Wi-Fi provisioning, microphone, speaker, wake word, repeated
conversation and reconnect. Observe the UART prefix and baud rate and test
stand (`kup`), sit (`ksit`) and rest (`d`) on a safely supported robot.
Also test malformed input, a full queue and a disconnected NyBoard. Record
the tested firmware SHA, SDK SHA and hardware revision with results.
