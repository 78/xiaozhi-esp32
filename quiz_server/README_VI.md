# Tài Liệu Kỹ Thuật: Xiaozhi Management Hub (Quiz Server V2)

Tài liệu này mô tả kiến trúc và hướng dẫn vận hành hệ thống **Xiaozhi Management Hub** - nền tảng quản lý tập trung cho tính năng Quiz Mode của robot Xiaozhi.

## 1. Kiến Trúc Mới (V2)

Hệ thống đã được nâng cấp từ một Server Quiz đơn giản thành một **Hub Quản Lý Đa Người Dùng**:

*   **Authentication**: Hệ thống Đăng ký / Đăng nhập bảo mật (JWT + BCrypt). Mỗi người dùng có tài khoản riêng.
*   **Database (S3-Based)**: Sử dụng AWS S3 làm "Database" lưu trữ JSON. Không tốn chi phí database server, dữ liệu tồn tại vĩnh viễn dù Server (Render) bị khởi động lại.
*   **Frontend Dashboard**: Giao diện trực quan (Vue.js) giúp quản lý thiết bị, chỉnh sửa câu hỏi (Visual Editor) và xem lịch sử thi.
*   **Multi-Tenancy**: Người dùng A không thể thấy thiết bị của Người dùng B.

## 2. Hướng Dẫn Cài Đặt Local (Cho Developer)

Để chạy server trên máy cá nhân:

### Bước 1: Yêu cầu
*   Node.js (v14 trở lên)
*   Tài khoản AWS S3 (Access Key, Secret Key, Bucket Name)

### Bước 2: Cài đặt Packages
Chạy lệnh sau để cài đặt đầy đủ các thư viện cần thiết (bao gồm JWT, BCrypt, AWS SDK, Express...):

```bash
npm install express body-parser cors multer uuid bcryptjs jsonwebtoken @aws-sdk/client-s3 @aws-sdk/lib-storage
```

Nếu bạn đã có file `package.json`, chỉ cần chạy:
```bash
npm install
```

### Bước 3: Cấu hình Môi Trường
Tạo biến môi trường (hoặc set trong terminal):
*   `AWS_ACCESS_KEY_ID`: Key S3 của bạn.
*   `AWS_SECRET_ACCESS_KEY`: Secret S3 của bạn.
*   `S3_BUCKET_NAME`: Tên bucket (ví dụ: `my-quiz-bucket`).
*   `AWS_REGION`: Vùng (ví dụ: `us-east-1`).
*   `JWT_SECRET`: Chuỗi bí mật bất kỳ để mã hóa token đăng nhập (ví dụ: `baomat123`).

### Bước 4: Chạy Server
```bash
node index.js
```
Truy cập: `http://localhost:3000`

---

## 3. Hướng Dẫn Deploy lên Render.com

Render là nền tảng hosting miễn phí lý tưởng cho Node.js server này. Vì Render reset ổ cứng sau mỗi lần deploy/restart, việc cấu hình S3 là **BẮT BUỘC** để không mất dữ liệu.

1.  **Tạo Web Service**: Kết nối Repo GitHub chứa code này.
2.  **Cấu hình Build/Start**:
    *   **Build Command**: `npm install`
    *   **Start Command**: `node index.js`
3.  **Environment Variables (Quan trọng)**:
    Vào tab **Environment** trên Render Dashboard và thêm các biến sau:

    | Key | Value (Ví dụ) |
    | :--- | :--- |
    | `AWS_ACCESS_KEY_ID` | `AKIAIOSFODNN7EXAMPLE` |
    | `AWS_SECRET_ACCESS_KEY` | `wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY` |
    | `S3_BUCKET_NAME` | `xiaozhi-quiz-data` |
    | `AWS_REGION` | `us-east-1` |
    | `JWT_SECRET` | `chuoi-bi-mat-sieu-dai-va-kho-doan` |

---

## 4. Hướng Dẫn Sử Dụng Dashboard

1.  **Đăng Nhập/Đăng Ký**: Truy cập trang chủ, tạo tài khoản mới.
2.  **Thêm Thiết Bị**:
    *   Trên Dashboard, nhấn **Add Device**.
    *   Nhập 6 ký tự cuối của MAC Address (in trên vỏ hộp hoặc xem trên log ESP32). Ví dụ: `A1B2C3`.
    *   Đặt tên gợi nhớ (ví dụ: "Robot Phòng Khách").
3.  **Chỉnh Sửa Bộ Câu Hỏi**:
    *   Nhấn vào thẻ thiết bị -> **Edit Quiz**.
    *   Thêm câu hỏi, nhập nội dung, 4 đáp án và chọn đáp án đúng.
    *   Nhấn **Save Changes**. Dữ liệu sẽ được lưu lên Cloud (S3).
4.  **Xem Lịch Sử**:
    *   Nhấn **History** để xem bảng thống kê các lần thi: Ngày giờ, Số điểm, Thời gian làm bài.

## 5. Cấu Trúc File Hệ Thống

*   `index.js`: Server chính, xử lý API và Auth.
*   `db_manager.js`: Quản lý kết nối S3, đọc/ghi file JSON (Users, Devices, Quizzes, History).
*   `public/dashboard.html`: Giao diện người dùng (Vue.js Single File).
*   `package.json`: Khai báo dependencies.

---
*Tài liệu được cập nhật ngày 20/01/2026 bởi Đội ngũ Phát triển Xiaozhi.*
