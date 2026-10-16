# Hi Lily N4R2 – Đồng bộ lịch bản tin sáng từ Cloudflare Master Worker v4.5.6

## Mô hình

- **Family Admin D1** lưu `morning_configs` và `morning_runs` theo `device_id`. ESP32 không được tự chọn hồ sơ Family khác.
- **Master Worker** `/api/device/morning/config` (GET) cấp đúng lịch của loa đã xác thực, có `server_unix` và `Asia/Ho_Chi_Minh`.
- **Master Worker** `/api/device/morning/event` (POST) gọi vào Admin D1 để **claim** lượt `started` theo `device_id + day`, chống phát nhiều lần một ngày; sau khi hoàn thành/hủy gửi `completed`, `interrupted`, `failed`.
- ESP32 không dùng `WORKER_TOKEN`, `FAMILY_SERVICE_TOKEN` hoặc quyền Admin. Mỗi loa có token riêng trong secret `MORNING_DEVICE_TOKENS`.

## 1. Deploy Master Worker

Tải mã `xiaozhi-master-v4.5.6-device-schedule.js` do ChatGPT cung cấp và dán **toàn bộ** vào Cloudflare Master Worker đang chạy (giữ nguyên các Worker bindings và tool khác).

Trong **Settings → Variables and Secrets**, thêm Secret **`MORNING_DEVICE_TOKENS`** dạng **chuỗi JSON**, ví dụ:
```json
{"vip_01":"PUT_RANDOM_64_HEX_CHARACTERS_HERE","vip_02":"ANOTHER_RANDOM_64_HEX_CHARACTER_TOKEN"}
```
**Không** điền chuỗi mẫu trên. Mỗi thiết bị bắt buộc có token khác nhau, ngẫu nhiên ≥32 ký tự; không đưa token thật vào GitHub. Trong Windows PowerShell 5.1, có thể tạo token 32 byte:
```powershell
$bytes = New-Object byte[] 32
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
$rng.GetBytes($bytes)
$token = -join ($bytes | ForEach-Object { $_.ToString("x2") })
$token
```
Duy trì **Service binding `FAMILY_ADMIN`** và Secret **`FAMILY_SERVICE_TOKEN`** sẵn có; chúng được Worker dùng để đọc D1. Không sửa Admin Worker ở bước này.

### Thử API bằng PowerShell (đổi địa chỉ Worker và mã/token thực tế)
```powershell
$base = "https://xiaozhi-master.nguyennha-020201.workers.dev"
$id = "vip_01"
$token = "TOKEN_RIENG_CUA_VIP_01"
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
