# Kevin Box 2 local firmware

This branch keeps the official cloud protocol while adding local dual-network control,
offline fixed commands, and a Wi-Fi-only administration panel. It does not deploy or
modify a backend.

## Network behavior

- `auto` prefers Wi-Fi. Wi-Fi gets a 20-second startup/recovery window.
- An active network must fail three health checks over at least 15 seconds before it is
  considered unavailable.
- A 4G candidate is committed only after registration and an HTTPS health check pass.
- After switching to 4G, the device remains there for at least two minutes. Wi-Fi must
  then pass two probes at least 30 seconds apart before it can take over again.
- At most two switches are allowed in ten minutes; exceeding that rate starts a
  five-minute cooldown.
- If both paths are unavailable, Wi-Fi local services and offline commands remain
  available. The device does not reboot in a loop.

No-SIM handling is intentionally conservative. A missing SIM ends the current modem
attempt immediately. The first three failed starts are separated by 30 seconds. After
the third failure, AUTO inhibits 4G switching and performs only one low-frequency SIM
presence check every five minutes. A successful HTTPS check clears the failure counter.
Explicitly selecting 4G is the user override that clears the breaker and retries at once.

## Device controls

The startup controls are:

- Hold volume up: fixed Wi-Fi mode.
- Hold volume down: fixed 4G mode.
- Double-click BOOT: AUTO mode.
- Click BOOT while starting/configuring: secure maintenance hotspot.
- Triple-click BOOT while starting/configuring: reset only the local panel administrator
  password, then enter the maintenance hotspot. Saved Wi-Fi networks are preserved.

In idle state, the OLED displays `Wi-Fi · 唤醒就绪`, `4G · 唤醒就绪`, or
`离线 · 唤醒就绪`. During a switch it displays the current and candidate transports plus
the reason. The LED uses low blue for Wi-Fi, low purple for 4G, slow yellow blink for
offline, and fast yellow blink while switching.

When the cloud path is unavailable, saying the configured WakeNet phrase opens a
five-second local MultiNet window. Supported commands are automatic network, Wi-Fi,
4G, network status, battery status, speaker mute, and speaker restore. Audio from this
window is not sent to the cloud.

## Local panel security

The maintenance hotspot uses a random WPA2 password shown only on the OLED. The first
administrator password must be created there and must contain 10 to 64 characters.
The device stores only a random salt and a PBKDF2-HMAC-SHA256 hash in NVS. Sessions
expire after 30 minutes and use HttpOnly/SameSite cookies, CSRF validation, and login
rate limiting.

The panel is available over the maintenance hotspot and, after a password exists, can
be enabled on the active Wi-Fi LAN. It is stopped before 4G becomes active and is never
exposed on the cellular WAN. Status responses exclude Wi-Fi passwords, administrator
passwords, IMEI, ICCID, device UUID, and cloud credentials.

## Build and capacity checks

Use ESP-IDF 6.0.2:

```sh
source "$IDF_PATH/export.sh"
python scripts/build.py kevin/box-2 --name kevin-box-2
python scripts/check_partition_sizes.py \
  --partition-table build/partition_table/partition-table.bin \
  --artifact ota_0=build/xiaozhi.bin \
  --artifact assets=build/generated_assets.bin
```

The pull-request CI also builds `minsi-k08-dual` and `yunliao-s3` to protect the shared
dual-network lifecycle.

## Backup and flash

Resolve the exact serial port before every command. The CH343 wrapper fixes RAM and
flash transfer blocks at 256 bytes.

Back up the complete 16 MiB flash and the NVS partition before the first custom flash:

```sh
python scripts/ch343_esptool.py --chip esp32s3 -p /dev/cu.EXACT_PORT \
  -b 460800 read-flash 0x0 0x1000000 full-flash-before-local.bin
python scripts/ch343_esptool.py --chip esp32s3 -p /dev/cu.EXACT_PORT \
  -b 460800 read-flash 0x9000 0x4000 nvs-before-local.bin
shasum -a 256 full-flash-before-local.bin nvs-before-local.bin
```

For routine installation or upgrade, preserve NVS and flash only the application and
assets partitions:

```sh
python scripts/ch343_esptool.py --chip esp32s3 -p /dev/cu.EXACT_PORT \
  -b 460800 --before default-reset --after hard-reset write-flash \
  --flash-mode dio --flash-size 16MB --flash-freq 80m \
  0x20000 build/xiaozhi.bin 0x800000 build/generated_assets.bin
```

`merged-binary.bin` is for recovery and factory-style installation. Writing it at
`0x0` overwrites the NVS region and therefore erases saved Wi-Fi and local settings.

## Rollback and release gate

To roll back while preserving user settings, flash the verified baseline application
and assets images at `0x20000` and `0x800000`. Restore the full-flash backup only when
the partition table or boot chain also needs recovery; doing so restores the old NVS
snapshot as well.

Create a release candidate with:

```sh
python scripts/package_kevin_box2_release.py \
  --build-dir build \
  --output-dir /path/to/kevin-box2-local-v0.1.0-rc1 \
  --release kevin-box2-local-v0.1.0-rc1 \
  --source-commit SOURCE_COMMIT \
  --upstream-commit UPSTREAM_COMMIT \
  --idf-version 6.0.2
```

The package remains `hardware-validation-pending` until cold modem detection,
registration, failover/recovery timing, 20 switch cycles, offline commands, maintenance
hotspot, panel authentication, and wake-word success-rate checks all pass. Only then may
it be repackaged as `kevin-box2-local-v0.1.0` with `--validation-status
hardware-validated`.
