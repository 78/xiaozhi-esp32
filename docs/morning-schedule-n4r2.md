# Hi Lily N4R2 – Đồng bộ lịch bản tin sáng từ Cloudflare Master Worker v4.5.7

## Mô hình

- **Family Admin D1** lưu `morning_configs` và `morning_runs` theo `device_id`. ESP32 không được tự chọn hồ sơ Family khác.
- **Master Worker** `/api/device/morning/config` (GET) cấp đúng lịch của loa đã xác thực, có `server_unix` và `Asia/Ho_Chi_Minh`.
- **Master Worker** `/api/device/morning/event` (POST) gọi vào Admin D1 để **claim** lượt `started` theo `device_id + day`, chống phát nhiều lần một ngày; sau khi hoàn thành/hủy gửi `completed`, `interrupted`, `failed`.
- ESP32 không dùng `WORKER_TOKEN`, `FAMILY_SERVICE_TOKEN` hoặc quyền Admin. Mỗi loa có một token phạm vi hẹp **được Master Worker tự tính từ `WORKER_TOKEN` đã có**. Không tạo thêm secret Cloudflare.

## 1. Deploy Master Worker

Tải mã `xiaozhi-master-v4.5.7-simplified-auth.js` do ChatGPT cung cấp và dán **toàn bộ** vào Cloudflare Master Worker đang chạy (giữ nguyên các Worker bindings và tool khác).

**Không thêm `MORNING_DEVICE_TOKENS` hoặc một khóa Cloudflare mới nào.** Nếu đã tạo biến này ở bản 4.5.6, có thể xóa khỏi Cloudflare sau khi v4.5.7 deploy thành công. Master dùng **`WORKER_TOKEN` hiện có** để tính HMAC-SHA256 theo đúng `device_id` và cấp một token riêng cho loa. Vẫn giữ Service binding `FAMILY_ADMIN` và secret `FAMILY_SERVICE_TOKEN` sẵn có. Không sửa Admin Worker.

### Lấy mã của một loa một lần – PowerShell

```powershell
$base = "https://xiaozhi-master.nguyennha-020201.workers.dev"
$id = "vip_01"
$master = Read-Host "Nhap WORKER_TOKEN dang su dung"
$token = (Invoke-RestMethod -Uri "$base/api/family/morning/provision-device" -Headers @{
  Authorization = "Bearer $master"
  "X-Xiaozhi-Device-ID" = $id
}).device_token
$master = $null
$token
```

- **`$token`** là token 64 ký tự của `vip_01`, không phải `WORKER_TOKEN`. Dùng nó trong firmware ESP32; **không ghi vào GitHub**.
- Muốn cấp cho loa khác, đổi `$id` thành `vip_02` rồi chạy lại với cùng `WORKER_TOKEN`. Mỗi loa nhận token khác nhau.
- Nếu đổi `WORKER_TOKEN` sau này, tất cả token thiết bị phải được cấp lại.
- Endpoint cấp token chỉ hoạt động khi đăng nhập bằng `WORKER_TOKEN` chính xác, không hoạt động với token thiết bị.

### Thử API bằng PowerShell
```powershell
Invoke-RestMethod -Uri "$base/api/device/morning/config" -Headers @{
  Authorization = "Bearer $token"
  "X-Xiaozhi-Device-ID" = $id
}
```
Phản hồi gồm `enabled`, `wake_time`, `active_days` (Chủ Nhật=0, Thứ Hai=1 ... Thứ Bảy=6), `duration_minutes`, `timezone`, `server_unix`. Sai token hoặc giả mạo ID -> HTTP 401. Không thử POST `started` vào D1 thật khi chưa muốn đánh dấu hôm đó đã chạy!

## 2. Build firmware ESP32-S3 N4R2 riêng của bạn

Mã ở nhánh `feat/esp32-s3-zero-n4r2-tft`. **Giữ nguyên** preset board `esp32-s3-zero-tft`, **4 MB Quad flash, 2 MB Quad PSRAM**, partition `partitions/v2/4m.csv`, âm thanh INMP441 + PCM5102A và màn hình TFT.

```shell
python scripts/build.py esp32-s3-zero-tft --name esp32-s3-zero-tft --language vi-VN
```

Sau khi script tạo cấu hình, chọn token riêng trên thiết bị qua `idf.py menuconfig`:
**Xiaozhi Assistant → Hi Lily Morning Device ID** = `vip_01`, **Hi Lily Morning Device Token** = token 64 hex ký tự của `vip_01`.

Sau đó chạy **`idf.py build`** và flash. Lưu ý `scripts/build.py` có thể tái tạo `sdkconfig`; nếu chạy script build lại, phải kiểm tra thiết lập token rồi chạy `idf.py build` lần nữa. Không commit `sdkconfig` có chứa bí mật.

Nếu cả ID và token trống, tính năng lịch sẽ **tắt hoàn toàn**, còn các chức năng hiện có vẫn hoạt động.

## 3. Nguyên tắc chạy

- Tác vụ HTTP **đã có** của màn hình lấy lịch khoảng mỗi 2 phút; không tạo task thứ hai cho TLS.
- Tính giờ bằng epoch từ chính Worker + đồng hồ đơn điệu ESP32, quy đổi sang UTC+7; không dùng giờ biên dịch firmware.
- Trong 10 phút kể từ `wake_time` và đúng `active_days`, chỉ khi ứng dụng ở `Idle`, micro/TTS không đang phát, nhạc không loading/playing/paused mới thử claim D1 và phát bản tin.
- Khi đến giờ mà đang hội thoại, thông báo hoặc phát nhạc: **không ngắt**, kiểm tra lại mỗi 5 giây trong cửa sổ 10 phút. Hết cửa sổ thì bỏ, không phát bù trễ.
- Worker Admin D1 chỉ cho `started` một lần/ngày/loa, kể cả sau khi khởi động lại.
- Khi phát thành công, gửi `listen.detect` mở chương trình, sau `tts.stop` + audio drained thì tự nối tiếp các phần 2–12.
- Người dùng gọi wake word, nhấn nút, mất Wi-Fi/WS hoặc timeout → hủy chuỗi; đưa trạng thái tương ứng vào hàng đợi sự kiện HTTP.
- Phần auto tự khởi phát cần WebSocket XiaoZhi; nếu firmware được cấp MQTT thay vì WSS, phần tự phát không kích hoạt.
- Phiên chưa đóng chính xác trong D1 do mất mạng sẽ được gửi trạng thái khi mạng phục hồi; có thể không gửi được nếu tắt nguồn trước khi mạng trở lại.

## 4. Kiểm thử trước khi dùng thật

1. `GET /api/device/morning/config` đúng token / đúng ID phải trả lịch trong D1; dùng token loa khác phải bị 401.
2. Đổi tạm `wake_time` trong Dashboard sang vài phút sau, bật ngày hiện tại theo **múi giờ Việt Nam**.
3. Trong giờ hẹn, không chạm loa: quan sát log `Morning radio scheduled start`.
4. Khi mở nhạc hoặc đang trò chuyện trong giờ hẹn, xác nhận không có chương trình chen ngang.
5. Gọi wake word khi đang phát: phải dừng và không tự đọc đoạn tiếp theo.
6. Ngắt Wi-Fi giữa chừng và bật lại, không được phát lại lượt đã claim trong D1.
7. Kiểm tra màn hình TFT, phát nhạc, WakeNet9, bộ nhớ PSRAM và dung lượng app/partition sau khi build.

**Chưa có kết quả biên dịch ESP-IDF thực tế hoặc kiểm tra phần cứng.** Không mặc định firmware đã vừa phân vùng 4 MB chỉ vì mã nguồn bổ sung tương đối nhỏ. Nếu build vượt flash/PSRAM, phải xử lý số liệu từ `idf.py size` trước khi flash.

## Prompt XiaoZhi

Tiếp tục dùng tài liệu `docs/morning-briefing-12-parts.md` của nhánh: mỗi lượt DeepSeek chỉ đọc một phần, các phần sau do ESP32 tự gửi `listen.detect`. Bản tin tự phát chỉ là một cách khởi chạy mới và vẫn dùng đúng prompt đó.
