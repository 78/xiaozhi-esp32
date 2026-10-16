# Hi Lily N4R2 — Ghép loa từ Dashboard, không cài token trong firmware

## Một firmware cho nhiều loa

Nạp cùng một firmware cho tất cả các bo ESP32-S3-Zero **4 MB Quad flash / 2 MB Quad PSRAM** có sơ đồ chân Hi Lily giống nhau. Khi khởi động lần đầu, mỗi bo sinh bí mật ngẫu nhiên 256 bit và mã PIN 8 chữ số, lưu vào NVS của chính bo đó. Không nhúng Device ID, WORKER_TOKEN hoặc FAMILY_SERVICE_TOKEN vào firmware.

## Triển khai

1. Deploy `xiaozhi-device-manager-v1.5.3-dashboard-pairing.js` lên Family Admin Worker đang dùng, giữ nguyên D1 và các secrets. API tự tạo bảng `speaker_pairings` khi có yêu cầu.
2. Deploy `xiaozhi-master-v4.5.8-dashboard-pairing.js` lên Master Worker, giữ nguyên binding `FAMILY_ADMIN` và secret `FAMILY_SERVICE_TOKEN`.
3. Build firmware từ nhánh `feat/esp32-s3-zero-n4r2-tft` bằng preset `esp32-s3-zero-tft`. Không cần chỉnh Device ID/token trong menuconfig.

## Cách dùng

1. Cấp Wi-Fi cho ESP32. Trên TFT sẽ hiển thị `ID: s3_...` và `MA: 12345678` (mã mỗi loa khác nhau).
2. Vào Dashboard Admin → Thiết bị → **Ghép loa với Dashboard**.
3. Chọn hồ sơ loa đã có, nhập đúng ID và mã 8 số đang hiển thị, nhấn **Duyệt và gán loa**.
4. Loa tự nhận trạng thái duyệt và đồng bộ giờ phát bản tin từ D1 của đúng hồ sơ; tính giờ theo `Asia/Ho_Chi_Minh`.

Chưa có hồ sơ thì vẫn cần tạo hồ sơ/nhập WSS endpoint của loa trên Dashboard theo quy trình hiện tại. Ghép nối mới **không tự đăng ký tài khoản xiaozhi.me**.

Mã PIN ghép nối hết hạn sau 10 phút, tối đa 5 lần nhập sai. Chỉ quản trị viên đăng nhập mới có thể duyệt/hủy ghép. Nếu Admin hủy ghép, loa mất quyền đọc lịch. Dữ liệu trong D1 chỉ lưu mã băm của bí mật, không lưu bí mật rõ.

## Lưu ý N4R2

- Giữ nguyên `partitions/v2/4m.csv`, WakeNet, TFT, INMP441/PCM5102A, các chân GPIO hiện có.
- Dùng lại HTTP task của màn hình thay vì tạo một TLS task mới.
- Cùng tệp .bin là dùng chung được, **nhưng không clone NVS** của bo A sang bo B.
- Thay firmware bằng flash ứng dụng giữ NVS thì vẫn giữ danh tính. Xóa hết flash/NVS sẽ phải ghép lại.
- Chưa build ESP-IDF hay kiểm thử trên bo thật. Phải xem `idf.py size` và log RAM/PSRAM trước khi xác nhận vừa 4 MB.

## Kiểm chứng

Cloudflare Master và Admin đã qua `node --check` cùng kiểm thử tích hợp mô phỏng hai loa. Các trường hợp mã PIN sai, bí mật sai và giả mạo thiết bị bị từ chối. Phần firmware chưa được biên dịch thực tế.
