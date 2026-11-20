# Petoi Bittle AI ESP32-C3

This board definition supports the Petoi custom ESP32-C3 voice board with
16 MB flash, an ES8311 codec, a 240 x 280 ST7789 display and a NyBoard UART
connection. Its OTA identity is `petoi-bittle-ai-c3`. Use the Petoi image
with this board's v2 partition layout.

The product variant displays animated eyes, ears and a mouth driven by
speaker PCM. Conversation text is hidden while provisioning and system
alerts remain visible. Five `self.expression.*` MCP tools control the
display. The product keeps Wi-Fi modem sleep off in low-power mode,
retains activation and connection configuration fetches, and disables
firmware installation. Robot motion starts with a command.

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
The tested BiBoard V1.0 uses the prefixed 9600-baud voice UART. With OpenCat
`260820`, sequence support requires forwarding the `d` token alongside
`k` and `m` on that UART.

## Build

Activate ESP-IDF `release/v6.1`, then use the canonical build entry point:

```sh
python scripts/build.py petoi/bittle-ai-c3 --name petoi-bittle-ai-c3 --language zh-CN --wake-word nihaoxiaozhi
```

An explicit Chinese build enables cached Chinese/English system text,
sounds and the corresponding Nihao Xiaozhi and Hi Jason wake models. An empty
`CONFIG_PETOI_COUNTRY_LOOKUP_URL` uses the cached language or Chinese on
a fresh device. Set the URL to an HTTPS country provider to enable automatic
selection on a new Wi-Fi network. The request contains no SSID. The cloud
controls the language of conversational replies.

The canonical build defaults to the standard compile-time English locale.
To choose English explicitly, use:

```sh
python scripts/build.py petoi/bittle-ai-c3 --name petoi-bittle-ai-c3 --language en-US --wake-word hiesp
```

Use `petoi-bittle-ai-c3-resource-debug` to enable DEBUG dialogue resource
snapshots. Use `petoi-bittle-ai-c3-locale-english-check` with
`--language zh-CN` to exercise Hi Jason and the English system resources
without writing the locale cache. The normal variant uses INFO logging.
The older Petoi firmware uses a different partition layout. Back up the
firmware and settings and use application and assets from the same v2 build
when migrating.

## Robot MCP tools

`self.robot.send_command` takes the string `text`. Examples are `ksit`
and `kup;ksit;d`. The product validates a single printable ASCII command or
a sequence of up to four supported actions. A two-entry queue feeds one
UART worker. Each step uses the `58 41 15` prefix without a newline.
The board can also recognize supported action phrases in Chinese or English
and use the corresponding result for a matching tool call in that turn.

Single-action replies report transmission. Sequences wait for each UART
token and stop on timeout without retry. A token confirms a UART observation.
Verify physical motion separately. Gait values below 200 specify steps.
Values from 200 specify milliseconds. `self.robot.get_status` reports
queue, transmission and token state. With `CONFIG_PETOI_ACTION_SEQUENCES`
disabled, the basic driver accepts single commands into a four-entry queue.

## Hardware verification

Build success alone is insufficient. Verify cold boot, display orientation,
backlight, button, Wi-Fi provisioning, microphone, speaker, wake word, repeated
conversation and reconnect. Observe the UART prefix and baud rate and test
stand (`kup`), sit (`ksit`) and rest (`d`) on a safely supported robot.
Also test malformed input, a full queue and a disconnected NyBoard. Record
the tested firmware SHA, SDK SHA and hardware revision with results.
