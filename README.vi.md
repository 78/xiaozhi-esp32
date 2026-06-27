# xiaozhi-esp32-vi 🇻🇳

Fork của [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) với:

- 🇻🇳 **Tiếng Việt làm UI mặc định** (locale vi-VN có sẵn từ upstream)
- 🎯 **Target board**: Waveshare ESP32-S3-RLCD-4.2
- 🤖 **Auto-build firmware** qua GitHub Actions

## Tải firmware

- **Latest release**: [Releases page](../../releases) (có file `.bin` + checksum SHA256)
- **Latest CI build**: [Actions tab](../../actions) — artifact `xiaozhi-vi-rlcd-4.2-*`

## Flash lên board

### Bước 1: Cài esptool

```bash
pip3 install --break-system-packages esptool
```

### Bước 2: Tìm USB port của board

Trên macOS:

```bash
ls /dev/cu.usbmodem*
```

### Bước 3: Flash

```bash
# Erase trước (clear NVS để pick up locale mới)
esptool.py --chip esp32s3 --port /dev/cu.usbmodem11301 --baud 460800 erase_flash

# Flash firmware
esptool.py --chip esp32s3 --port /dev/cu.usbmodem11301 --baud 115200 \
    write_flash --no-compress 0x0 merged-binary.bin
```

### Bước 4: Verify

Mở Serial Monitor để xem boot log:

```bash
screen /dev/cu.usbmodem11301 115200
# Thoát: Ctrl+A, K, Y
```

Sau khi boot, UI sẽ hiển thị tiếng Việt: "Chờ", "Đang kết nối...", "Đang lắng nghe...", ...

## Sync với upstream

```bash
git fetch upstream
git checkout main
git merge upstream/main
git push origin main
```

## Build local (tùy chọn, không cần)

GitHub Actions tự build, nhưng nếu muốn build local:

```bash
# Cài ESP-IDF v5.5.2
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp-idf
cd ~/esp-idf && git checkout v5.5.2 && ./install.sh esp32s3
source ~/esp-idf/export.sh

# Build
cd path/to/xiaozhi-esp32-vi
python scripts/release.py waveshare/esp32-s3-rlcd-4.2 --name esp32-s3-rlcd-4.2-vi

# Output: build/merged-binary.bin
```

## CI Workflows

| Workflow | File | Trigger |
|---|---|---|
| Build VI Firmware | `.github/workflows/build-vi.yml` | push main/vi-build-rlcd, PR, manual dispatch |
| Release VI Firmware | `.github/workflows/release-vi.yml` | tag push `v*-vi*` |
| Upstream build (disabled) | `.github/workflows/build.yml` | workflow_dispatch only |

## License

Inherits MIT License from upstream [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32).
