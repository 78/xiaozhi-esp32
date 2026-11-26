# JC8048W550: dương lịch + âm lịch Việt Nam trên thẻ đầu màn hình chờ

## Mục đích
Người dùng muốn nhìn màn hình chờ là biết hôm nay là ngày giỗ hoặc lễ nào, kèm thông tin âm lịch Việt Nam đầy đủ. Đồng hồ HH:MM không còn cần thiết.

## Goals
- Bỏ đồng hồ HH:MM. Chỗ đó hiển thị ngày/tháng/năm dương lịch và thứ trong tuần.
- Dòng ngày hiện tại đổi thành âm lịch: ngày/tháng âm (đánh dấu tháng nhuận), năm can chi, ngày can chi, tháng can chi, tiết khí.
- Dòng sự kiện: chỉ hiện khi hôm nay có giỗ hoặc lễ chung (xem bảng bên dưới), ẩn vào ngày thường.
- Giữ nguyên toàn bộ phần thời tiết (hôm nay + 7 ngày), danh sách GitHub và chi tiết repo.

## Non-goals / anti-goals
- Không giỗ riêng của gia đình, không NVS/Kconfig mới.
- Không giờ hoàng đạo, ngày hoàng/hắc đạo, sao, trực (người dùng không yêu cầu).
- Không gọi mạng thêm cho âm lịch. Không đổi pin, board identity, protocol, core.
- Không viết chữ Việt không dấu.

## Ràng buộc
- Mọi thay đổi nằm trong `main/boards/guition/jc8048w550/`.
- Thuật toán âm lịch là hàm thuần (không LVGL, không mạng), múi giờ UTC+7, chạy được bằng self-test trên host như `dashboard_model_selftest.cc`.
- Đồng hồ chưa đồng bộ (năm < 2025): ẩn phần lịch như hiện nay ẩn đồng hồ, không hiện ngày sai.
- Hiển thị làm mới theo timer 10 giây hiện có.

## Hướng đã cân nhắc
Font tiếng Việt:
- A. Font "text" trong assets (Noto common): **bị loại**. Kiểm tra charset cho thấy thiếu `Ơ Ư ẵ ẹ ỡ ỳ ỵ ỷ ỹ` (thiếu `ỵ` làm hỏng chi Tỵ); bộ basic còn thiếu nhiều hơn.
- B. Nhúng font LVGL chứa Latin + Latin mở rộng tiếng Việt vào firmware: **được chọn**. Chắc chắn đủ glyph, không phụ thuộc assets partition hay gói emoji tuỳ chỉnh của người dùng.
- C. Không dấu: loại.

Dữ liệu sự kiện: bảng cố định trong code (lễ chung, cả âm lịch lẫn dương lịch), không có danh sách riêng.

## Thiết kế
- `lunar_calendar.{h,cc}` (mới): chuyển dương sang âm theo thuật toán Hồ Ngọc Đức (UTC+7); trả về ngày, tháng, nhuận, năm, can chi ngày/tháng/năm, tiết khí.
- `vn_events.{h,cc}` (mới): bảng sự kiện theo ngày/tháng âm hoặc dương: mùng 1, rằm, Tết Nguyên Đán (1/1), Tết Nguyên Tiêu (15/1), Giỗ Tổ Hùng Vương (10/3), Phật Đản (15/4), Đoan Ngọ (5/5), Vu Lan (15/7), Trung Thu (15/8), Ông Công Ông Táo (23/12), 30/4, 1/5, 2/9, 1/1 dương lịch. Có thể một ngày nhiều sự kiện.
- Font Việt: file C do `lv_font_conv` tạo ra, thêm vào danh sách nguồn của board; thêm cỡ cần dùng (cỡ ngày và cỡ dòng âm lịch). Ghi cách tạo lại trong README.
- `idle_dashboard.cc`: thay `clock_` bằng nhãn ngày dương lịch; thay `date_` bằng khối âm lịch + dòng sự kiện; `RefreshClock()` tính lại cả hai.
- Self-test: kiểm tra một tập ngày mốc đã biết (Tết các năm, tháng nhuận, đổi năm can chi).
- README của board: cập nhật mục màn hình chờ.

## Việc để người làm quyết định
- Cỡ chữ và vị trí chính xác, miễn đủ vừa thẻ và không đẩy danh sách GitHub xuống.
- Cách tạo font (công cụ `lv_font_conv` hay script có sẵn trong component fonts).
- Tên tiếng Việt chính xác của 24 tiết khí và 10 can, 12 chi.

## Kiểm thử và giới hạn
- Host/self-test cho thuật toán âm lịch và bảng sự kiện.
- Build `guition/jc8048w550` bằng IDF 6.1.
- Cần thiết bị thật để xác nhận chữ hiển thị và bố cục.
