# Tiểu Trí – Bản tin sáng chia 12 đoạn trên ESP32-S3

**Áp dụng:** nhánh `feat/esp32-s3-zero-n4r2-tft`, server `xiaozhi.me` đã được người dùng xác nhận có hỗ trợ `listen.detect` chứa văn bản. Không thay Master MCP, Gateway hoặc dịch vụ TTS.

## Bắt buộc cập nhật prompt nhân vật DeepSeek trên xiaozhi.me

Khi người dùng yêu cầu **bản tin sáng**, hãy gọi `family.get_daily_guidance` và `cloud.get_daily_briefing` nếu có, nhưng **chỉ đọc đoạn số 1 trong 12 đoạn**: chào ngày, ngày dương/âm, thời tiết hiện tại và điểm cần lưu ý. Viết khoảng 100–150 từ, không chào kết thúc, không hỏi có muốn tiếp tục.

Khi nhận câu văn bản bắt đầu bằng `TT_BRIEF_AUTO`, hiểu đó là **lệnh nội bộ từ ESP32**, không đọc mã này thành tiếng. Chỉ phát **đoạn được yêu cầu** (100–150 từ) thuộc cùng bản tin; không đọc lại đoạn trước, không mở đầu từ đầu và không hỏi người nghe có muốn tiếp tục. Dùng dữ liệu Family và nguồn MCP của cùng ngữ cảnh; nếu mất ngữ cảnh, tra lại khi cần. **Không bịa tin, lịch ngày, ngày giỗ, giờ tốt, thành viên hoặc thông số thời tiết.** Tuân thủ quyền riêng tư và địa điểm Family. Đọc tin tức chỉ ở nhịp bốn; nhịp năm là lời chúc cuối.

Các phần:
1. Chào ngày, âm lịch, thời tiết hiện tại;
2. Thời tiết theo giờ, AQI, ứng dụng sinh hoạt;
3. Lịch âm, Can Chi, giờ tốt, hướng xuất hành;
4. Ý nghĩa lịch ngày và khuyến nghị truyền thống;
5. Ngày lễ và chuẩn bị giỗ/Rằm/Mùng Một;
6. Lịch chung của gia đình;
7. Lời nhắn thành viên có sự kiện;
8. Lời nhắn thành viên còn lại, nếu có dữ liệu;
9. Tin 1 và 2;
10. Tin 3 và 4;
11. Tin 5 nếu có đủ nguồn;
12. Lời chúc kết thúc.

## Hoạt động trên thiết bị

- Nói `Đọc bản tin sáng` đúng một lần; firmware nhận STT và bật chế độ 12 phần.
- Nhận `tts.stop` từ server **và** `IsPlaybackIdle()` tại ESP32 thì mới gửi `SendWakeWordDetected(text)` cho phần tiếp theo.
- Chống lặp bằng cách tăng chỉ số phần trước khi gửi.
- Khi nhận wake word thật, người dùng nhấn nút hoặc bị lỗi kết nối, firmware hủy chuỗi; không cố phát tiếp sau reconnect.
- Watchdog 120 giây chờ server trả lời và 240 giây chờ kết thúc lượt TTS.
- Hoàn tất 12 phần thì hủy trạng thái. Thời lượng 12 phút là mục tiêu biên tập, không phải thời gian đo thực tế.
- **Không bao gồm** lịch tự phát 06:30: chức năng này cần triển khai riêng.

## Nghiệm thu

1. Flash ESP32-S3, mở log serial và nói `Đọc bản tin sáng`. Tìm `Morning radio started: 12 parts`.
2. Đảm bảo `Morning radio auto-continue part 2/12` chỉ xuất hiện sau khi thiết bị phát hết nhịp một.
3. Kiểm tra đủ 12 phần, tin ở phần 9–11, lời kết ở phần 12, không phát lặp.
4. Thử wake word khi đang phát; phải thấy hủy chuỗi, không có yêu cầu phần tiếp theo.
5. Thử ngắt mạng giữa chừng; không tự phát tiếp khi reconnect.
6. Đảm bảo nhạc và màn hình hoạt động như trước.

Nếu server không bảo toàn ngữ cảnh giữa những lượt `listen.detect`, DeepSeek cần gọi lại nguồn dữ liệu trong từng lượt cần thiết. Không khẳng định đã chạy thành công trên phần cứng khi chưa kiểm thử.
