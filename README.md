# Chatbot Dựa trên MCP

（Tiếng Việt | [中文](README_zh.md) | [English](README_en.md) | [日本語](README_ja.md)）

## Giới thiệu

👉 [Con người: Lắp camera cho AI vs AI: Phát hiện ngay chủ nhân không gội đầu 3 ngày【bilibili】](https://www.bilibili.com/video/BV1bpjgzKEhd/)

👉 [Tự tay chế tạo bạn gái AI của bạn, hướng dẫn cho người mới bắt đầu【bilibili】](https://www.bilibili.com/video/BV1XnmFYLEJN/)

Chatbot AI Xiaozhi như một giao diện tương tác bằng giọng nói, sử dụng khả năng AI của các mô hình lớn như Qwen / DeepSeek, thực hiện điều khiển đa thiết bị thông qua giao thức MCP.

<img src="docs/mcp-based-graph.jpg" alt="Điều khiển vạn vật thông qua MCP" width="320">

### Thông tin phiên bản

Phiên bản v2 hiện tại không tương thích với bảng phân vùng của phiên bản v1, vì vậy không thể nâng cấp từ phiên bản v1 lên phiên bản v2 thông qua OTA. Thông tin bảng phân vùng xem tại [partitions/v2/README.md](partitions/v2/README.md).

Tất cả phần cứng sử dụng phiên bản v1 có thể nâng cấp lên phiên bản v2 thông qua việc nạp firmware thủ công.

Phiên bản ổn định của v1 là 1.9.2, có thể chuyển sang phiên bản v1 thông qua `git checkout v1`, nhánh này sẽ được duy trì liên tục đến tháng 2 năm 2026.

### Tính năng đã triển khai

- Wi-Fi / ML307 Cat.1 4G
- Đánh thức bằng giọng nói offline [ESP-SR](https://github.com/espressif/esp-sr)
- Hỗ trợ hai giao thức truyền thông ([Websocket](docs/websocket.md) hoặc MQTT+UDP)
- Sử dụng codec âm thanh OPUS
- Tương tác bằng giọng nói dựa trên kiến trúc ASR + LLM + TTS streaming
- Nhận dạng giọng nói, xác định danh tính người nói hiện tại [3D Speaker](https://github.com/modelscope/3D-Speaker)
- Màn hình hiển thị OLED / LCD, hỗ trợ hiển thị biểu cảm
- Hiển thị pin và quản lý nguồn
- Hỗ trợ đa ngôn ngữ (tiếng Trung, tiếng Anh, tiếng Nhật)
- Hỗ trợ nền tảng chip ESP32-C3, ESP32-S3, ESP32-P4
- Điều khiển thiết bị thông qua MCP phía thiết bị (âm lượng, đèn LED, motor, GPIO, v.v.)
- Mở rộng khả năng mô hình lớn thông qua MCP đám mây (điều khiển nhà thông minh, thao tác desktop PC, tìm kiếm kiến thức, gửi nhận email, v.v.)
- Tùy chỉnh từ đánh thức, phông chữ, biểu cảm và nền chat, hỗ trợ chỉnh sửa trực tuyến qua web ([Bộ tạo Assets tùy chỉnh](https://github.com/78/xiaozhi-assets-generator))

## Phần cứng

### Thực hành chế tạo thủ công trên breadboard

Xem chi tiết trong hướng dẫn Feishu:

👉 [《Bách khoa toàn thư Chatbot AI Xiaozhi》](https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb?from=from_copylink)

Hình ảnh hiệu quả breadboard như sau:

![Hình ảnh hiệu quả breadboard](docs/v1/wiring2.jpg)

### Hỗ trợ hơn 70 phần cứng mã nguồn mở (chỉ hiển thị một phần)

- <a href="https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-shi-zhan-pai-esp32-s3-kai-fa-ban" target="_blank" title="Bo mạch phát triển ESP32-S3 Thực chiến Lichuang">Bo mạch phát triển ESP32-S3 Thực chiến Lichuang</a>
- <a href="https://github.com/espressif/esp-box" target="_blank" title="Espressif ESP32-S3-BOX3">Espressif ESP32-S3-BOX3</a>
- <a href="https://docs.m5stack.com/zh_CN/core/CoreS3" target="_blank" title="M5Stack CoreS3">M5Stack CoreS3</a>
- <a href="https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base" target="_blank" title="AtomS3R + Echo Base">M5Stack AtomS3R + Echo Base</a>
- <a href="https://gf.bilibili.com/item/detail/1108782064" target="_blank" title="Nút thần kỳ 2.4">Nút thần kỳ 2.4</a>
- <a href="https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm" target="_blank" title="Waveshare ESP32-S3-Touch-AMOLED-1.8">Waveshare ESP32-S3-Touch-AMOLED-1.8</a>
- <a href="https://github.com/Xinyuan-LilyGO/T-Circle-S3" target="_blank" title="LILYGO T-Circle-S3">LILYGO T-Circle-S3</a>
- <a href="https://oshwhub.com/tenclass01/xmini_c3" target="_blank" title="Xiage Mini C3">Xiage Mini C3</a>
- <a href="https://oshwhub.com/movecall/cuican-ai-pendant-lights-up-y" target="_blank" title="Movecall CuiCan ESP32S3">Mặt dây chuyền AI CuiCan</a>
- <a href="https://github.com/WMnologo/xingzhi-ai" target="_blank" title="Nologo Xingzhi-1.54 Công nghệ vô danh">Nologo Xingzhi-1.54TFT Công nghệ vô danh</a>
- <a href="https://www.seeedstudio.com/SenseCAP-Watcher-W1-A-p-5979.html" target="_blank" title="SenseCAP Watcher">SenseCAP Watcher</a>
- <a href="https://www.bilibili.com/video/BV1BHJtz6E2S/" target="_blank" title="Chó robot siêu tiết kiệm ESP-HI">Chó robot siêu tiết kiệm ESP-HI</a>

<div style="display: flex; justify-content: space-between;">
  <a href="docs/v1/lichuang-s3.jpg" target="_blank" title="Bo mạch phát triển ESP32-S3 Thực chiến Lichuang">
    <img src="docs/v1/lichuang-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/espbox3.jpg" target="_blank" title="Espressif ESP32-S3-BOX3">
    <img src="docs/v1/espbox3.jpg" width="240" />
  </a>
  <a href="docs/v1/m5cores3.jpg" target="_blank" title="M5Stack CoreS3">
    <img src="docs/v1/m5cores3.jpg" width="240" />
  </a>
  <a href="docs/v1/atoms3r.jpg" target="_blank" title="AtomS3R + Echo Base">
    <img src="docs/v1/atoms3r.jpg" width="240" />
  </a>
  <a href="docs/v1/magiclick.jpg" target="_blank" title="Nút thần kỳ 2.4">
    <img src="docs/v1/magiclick.jpg" width="240" />
  </a>
  <a href="docs/v1/waveshare.jpg" target="_blank" title="Waveshare ESP32-S3-Touch-AMOLED-1.8">
    <img src="docs/v1/waveshare.jpg" width="240" />
  </a>
  <a href="docs/v1/lilygo-t-circle-s3.jpg" target="_blank" title="LILYGO T-Circle-S3">
    <img src="docs/v1/lilygo-t-circle-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/xmini-c3.jpg" target="_blank" title="Xiage Mini C3">
    <img src="docs/v1/xmini-c3.jpg" width="240" />
  </a>
  <a href="docs/v1/movecall-cuican-esp32s3.jpg" target="_blank" title="CuiCan">
    <img src="docs/v1/movecall-cuican-esp32s3.jpg" width="240" />
  </a>
  <a href="docs/v1/wmnologo_xingzhi_1.54.jpg" target="_blank" title="Nologo Xingzhi-1.54 Công nghệ vô danh">
    <img src="docs/v1/wmnologo_xingzhi_1.54.jpg" width="240" />
  </a>
  <a href="docs/v1/sensecap_watcher.jpg" target="_blank" title="SenseCAP Watcher">
    <img src="docs/v1/sensecap_watcher.jpg" width="240" />
  </a>
  <a href="docs/v1/esp-hi.jpg" target="_blank" title="Chó robot siêu tiết kiệm ESP-HI">
    <img src="docs/v1/esp-hi.jpg" width="240" />
  </a>
</div>

## Phần mềm

### Nạp firmware

Người mới bắt đầu lần đầu thao tác khuyên nên không xây dựng môi trường phát triển trước, sử dụng trực tiếp firmware nạp không cần môi trường phát triển.

Firmware mặc định kết nối với máy chủ chính thức [xiaozhi.me](https://xiaozhi.me), người dùng cá nhân đăng ký tài khoản có thể sử dụng miễn phí mô hình thời gian thực Qwen.

👉 [Hướng dẫn nạp firmware cho người mới](https://ccnphfhqs21z.feishu.cn/wiki/Zpz4wXBtdimBrLk25WdcXzxcnNS)

### Môi trường phát triển

- Cursor hoặc VSCode
- Cài đặt plugin ESP-IDF, chọn phiên bản SDK 5.4 trở lên
- Linux tốt hơn Windows, tốc độ biên dịch nhanh, cũng tránh được phiền toái về vấn đề driver
- Dự án này sử dụng style code C++ của Google, khi submit code vui lòng đảm bảo tuân thủ quy chuẩn

### Tài liệu dành cho nhà phát triển

- [Hướng dẫn bo mạch tùy chỉnh](docs/custom-board.md) - Học cách tạo bo mạch phát triển tùy chỉnh cho Xiaozhi AI
- [Hướng dẫn sử dụng điều khiển IoT giao thức MCP](docs/mcp-usage.md) - Hiểu cách điều khiển thiết bị IoT thông qua giao thức MCP
- [Quy trình tương tác giao thức MCP](docs/mcp-protocol.md) - Cách triển khai giao thức MCP phía thiết bị
- [Tài liệu giao thức truyền thông hỗn hợp MQTT + UDP](docs/mqtt-udp.md)
- [Tài liệu chi tiết giao thức truyền thông WebSocket](docs/websocket.md)

## Cấu hình mô hình lớn

Nếu bạn đã sở hữu một thiết bị chatbot Xiaozhi AI và đã kết nối với máy chủ chính thức, có thể đăng nhập vào bảng điều khiển [xiaozhi.me](https://xiaozhi.me) để cấu hình.

👉 [Video hướng dẫn thao tác backend (giao diện cũ)](https://www.bilibili.com/video/BV1jUCUY2EKM/)

## Các dự án mã nguồn mở liên quan

Để triển khai máy chủ trên máy tính cá nhân, có thể tham khảo các dự án mã nguồn mở bên thứ ba sau:

- [xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server) Máy chủ Python
- [joey-zhou/xiaozhi-esp32-server-java](https://github.com/joey-zhou/xiaozhi-esp32-server-java) Máy chủ Java
- [AnimeAIChat/xiaozhi-server-go](https://github.com/AnimeAIChat/xiaozhi-server-go) Máy chủ Golang

Các dự án client bên thứ ba sử dụng giao thức truyền thông Xiaozhi:

- [huangjunsen0406/py-xiaozhi](https://github.com/huangjunsen0406/py-xiaozhi) Client Python
- [TOM88812/xiaozhi-android-client](https://github.com/TOM88812/xiaozhi-android-client) Client Android
- [100askTeam/xiaozhi-linux](http://github.com/100askTeam/xiaozhi-linux) Client Linux được cung cấp bởi 100ask Technology
- [78/xiaozhi-sf32](https://github.com/78/xiaozhi-sf32) Firmware chip Bluetooth của Siche Technology
- [QuecPython/solution-xiaozhiAI](https://github.com/QuecPython/solution-xiaozhiAI) Firmware QuecPython được cung cấp bởi Quectel

## Về dự án

Đây là một dự án ESP32 mã nguồn mở bởi Xiage, được phát hành dưới giấy phép MIT, cho phép bất kỳ ai sử dụng miễn phí, chỉnh sửa hoặc sử dụng cho mục đích thương mại.

Chúng tôi hy vọng thông qua dự án này có thể giúp mọi người hiểu về phát triển phần cứng AI, áp dụng các mô hình ngôn ngữ lớn đang phát triển nhanh chóng hiện nay vào các thiết bị phần cứng thực tế.

Nếu bạn có bất kỳ ý tưởng hoặc đề xuất nào, vui lòng tạo Issues hoặc tham gia nhóm QQ: 1011329060

## Lịch sử Star

<a href="https://star-history.com/#78/xiaozhi-esp32&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
   <img alt="Biểu đồ Lịch sử Star" src="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
 </picture>
</a>