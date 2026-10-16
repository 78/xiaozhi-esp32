# Hi Lily N4R2: morning schedule (minimal)

- One generic ESP32-S3-Zero N4R2 firmware: 4MB Quad flash + 2MB Quad PSRAM. No device token and no pairing PIN.
- Deploy xiaozhi-master-v4.5.9-simple-schedule.js to existing Master Worker. Keep FAMILY_ADMIN service binding and FAMILY_SERVICE_TOKEN secret unchanged. No Admin Worker update required.
- Firmware reads GET /api/device/morning/config?device_id=s3_xxxxxxxxxxxx from Master Worker; public API only exposes enabled, wake_time, active_days, timezone and server_unix.
- The ESP32 hardware identifier is printed as Dashboard morning schedule device ID: s3_xxxxxxxxxxxx in serial and briefly shown on TFT when Idle.
- Use the existing Dashboard Add device workflow with this same ID and configure a valid XiaoZhi WSS endpoint. Schedule is stored in existing morning_configs under the exact device ID. Existing vip_01 profiles are NOT automatically mapped or migrated.
- Task shares existing Hi Lily Home HTTP worker and checks Worker time in UTC plus seven hours for Asia/Ho_Chi_Minh.
- Morning program starts only if idle, playback drained and music not loading/playing/paused; postpones up to 10 minutes.
- Device saves YYYY-MM-DD in NVS before attempting scheduled start; no replay on same day after reboot. No server-side event queue or D1 run claim.
- No new private device keys or config symbols; prior family/admin service authentication unchanged.
- Manual 12-part listen.detect chain still waits for both tts.stop and local playback drain.
- Hardware firmware build and flash-size verification are NOT yet performed. Run python scripts/build.py esp32-s3-zero-tft --name esp32-s3-zero-tft --language vi-VN followed by idf.py size.
- Reminder: shared BIN must not include copied NVS. NVS on each board remains separate. OTA and physical mic/display behavior require real-device testing.