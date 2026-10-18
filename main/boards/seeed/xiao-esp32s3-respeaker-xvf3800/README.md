# ReSpeaker XVF3800 USB 4-Mic Array + XIAO ESP32-S3

This profile targets the circular four-microphone ReSpeaker XVF3800 USB Array
connected to a XIAO ESP32-S3. The XVF3800 runs its own I2S firmware; the Xiaozhi
firmware is built and flashed separately to the ESP32-S3.

## XMOS firmware and clock profiles

The I2S clock role and sample rate must match the running XMOS firmware.

| Build variant | ESP32-S3 role | I2S rate | Required XMOS mode |
| --- | --- | --- | --- |
| `xiao-esp32s3-respeaker-xvf3800` | Master | 16 kHz | Slave, 16 kHz |
| `xiao-esp32s3-respeaker-flex-circular-48k` | Slave | 48 kHz | Master, 48 kHz |

The second variant enables `CONFIG_RESPEAKER_I2S_EXTERNAL_CLOCK_48K`. It targets
Flex circular units observed to supply approximately 48 kHz WS while the XIAO
clock pins are inputs. Do not assume all Flex firmware has this clock mode:
the downloadable XMOS I2S slave images require a host master instead.
In external-clock mode the existing Xiaozhi audio service resamples the 48 kHz
microphone input to 16 kHz and playback to the codec's 48 kHz rate.

## Wiring

| Signal | XIAO ESP32-S3 GPIO |
| --- | --- |
| I2S BCLK | 8 |
| I2S WS/LRCLK | 7 |
| I2S ESP32-S3 output to XVF3800 | 44 |
| I2S XVF3800 output to ESP32-S3 | 43 |
| I2C SDA | 5 |
| I2C SCL | 6 |
| Ground | Ground |

The board-specific codec uses 32-bit stereo I2S at the selected clock rate.
Xiaozhi's mono playback is duplicated into both I2S slots;
the ReSpeaker's onboard codec/amplifier drives its single speaker connector.
Capture selects the left I2S slot as one processed microphone channel, rather
than feeding the two processed outputs into ESP-SR's two-microphone BSS.
Capture converts signed 32-bit PCM to 16-bit PCM with a 16-bit right shift,
without the original fork's additional 16x gain.
For hardware testing, the `RX:` log reports both raw slot peaks, the selected
channel's nonzero/full-scale sample counts, 16-bit RMS, and read timeouts
approximately every five seconds while recording is active. A second line
shows four raw left/right sample pairs in hexadecimal to help diagnose data
alignment or clock problems. These counters reset after each report.
GPIO 43 and 44 are also the default UART0 pins, so the board build selects USB
Serial/JTAG for the console. Keep the shared ground and verify the ReSpeaker
is running its I2S firmware before testing audio. The build profile selects an
8 MB flash layout.

## Build

Set the target to `esp32s3`, then select **Seeed XIAO ESP32-S3 + ReSpeaker
XVF3800 USB 4-Mic Array** in `idf.py menuconfig` under **Xiaozhi Assistant →
Target Board**. Build and flash with:

```powershell
idf.py build
idf.py flash monitor
```

The board has no required display; Xiaozhi uses the built-in `NoDisplay`
implementation.

## Passive clock diagnostic

For unknown XMOS factory firmware, enable
`CONFIG_RESPEAKER_I2S_CLOCK_PROBE_ONLY` to build a diagnostic-only image.
It leaves BCLK/WS as GPIO inputs without initializing I2S and reports clock
activity every two seconds. The polling-based WS rate is approximate, and the
BCLK edge count only indicates sampled activity, not the BCLK frequency.
No observed edges do not prove the XMOS firmware is an I2S slave: it may be
muted, inactive, unpowered, or waiting for initialization.

This mode does not start Wi-Fi, audio recognition, or chat. Disable the option
and rebuild to return to normal firmware. Fully power-cycle both the XIAO and
XMOS before testing so an earlier application's I2S state is not retained.
