# Hướng dẫn Build và Flash Firmware (Xiaozhi ESP32)

Tài liệu này hướng dẫn cách build firmware từ mã nguồn, flash vào thiết bị và gộp các file binary thành một file duy nhất để sử dụng cho mục đích thương mại hoặc sản xuất.

## 1. Dọn dẹp bản build cũ
Trước khi bắt đầu một bản build mới sạch sẽ, bạn nên xóa thư mục build cũ:
```bash
rm -rf build
```

## 2. Thiết lập mục tiêu (Target)
Thiết lập chip mục tiêu là ESP32-S3 (phù hợp với phần cứng Xiaozhi):
```bash
idf.py set-target esp32s3
```

## 3. Biên dịch mã nguồn (Build)
Chạy lệnh sau để bắt đầu quá trình biên dịch:
```bash
idf.py build
```

## 4. Flash Firmware vào thiết bị
Sau khi build thành công, dùng lệnh sau để flash firmware vào board thông qua cổng USB.
> **Lưu ý:** Thay đổi `/dev/cu.usbmodem5B7A1557141` bằng cổng serial tương ứng trên máy của bạn.

```bash
idf.py -p /dev/cu.usbmodem5B7A1557141 -b 115200 flash
```

## 5. Tạo file Binary tổng hợp (.bin)
Để tạo một file `.bin` duy nhất chứa tất cả các thành phần (bootloader, partition table, firmware, assets), sử dụng công cụ `esptool.py`:

```bash
esptool.py --chip esp32s3 merge_bin \
  -o xiaozhi_commercial_v1.bin \
  --flash_mode dio \
  --flash_size 16MB \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x20000 build/xiaozhi.bin \
  0x800000 build/generated_assets.bin
```

### Giải thích các thành phần:
- `-o xiaozhi_commercial_v1.bin`: Tên file đầu ra.
- `--flash_mode dio`: Chế độ flash.
- `--flash_size 16MB`: Kích thước flash (16MB cho ESP32-S3 N16R8).
- `0x0`: Địa chỉ cho Bootloader.
- `0x8000`: Địa chỉ cho Partition Table.
- `0xd000`: Địa chỉ cho OTA data initial.
- `0x20000`: Địa chỉ cho App firmware (xiaozhi.bin).
- `0x800000`: Địa chỉ cho Generated Assets (Partition chứa dữ liệu hình ảnh/âm thanh).

---
*Chúc bạn thành công!*
