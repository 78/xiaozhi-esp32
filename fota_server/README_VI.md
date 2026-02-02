# Hướng Dẫn Triển Khai Xiaozhi FOTA Server

Dự án này cung cấp một Web Server hoàn chỉnh để quản lý và cập nhật firmware từ xa (FOTA) cho các thiết bị Xiaozhi.

## 1. Kiến Trúc Hệ Thống

Hệ thống bao gồm 2 phần chính:

*   **Frontend (Next.js)**: Dashboard quản trị, giao diện đẹp (theo UX/UI Concept), cho phép quản lý thiết bị và upload firmware.
*   **Backend (Node.js/Express)**: API Server, kết nối Database, tích hợp MQTT để gửi lệnh Push Update tới robot, và tích hợp AWS S3 để lưu trữ firmware.

## 2. Công Nghệ Sử Dụng

*   **Frontend**: Next.js, Tailwind CSS (Violet Theme), Socket.io Client.
*   **Backend**: Node.js, Express, MQTT.js, AWS SDK (S3), SQLite (đơn giản hóa việc lưu trữ, có thể đổi sang MongoDB/PostgreSQL).
*   **Deployment**: Hỗ trợ deploy lên các nền tảng miễn phí (Vercel, Render).

## 3. Xác Thực Thiết Bị

Theo phân tích codebase, phương pháp định danh tối ưu nhất là **MAC Address**.
*   Khi thiết bị kết nối, MAC Address là định danh duy nhất không thay đổi.
*   Token WSS có thể thay đổi sau mỗi session hoặc reset.

## 4. Hướng Dẫn Cài Đặt & Chạy (Local)

### 4.1. Backend

```bash
cd backend
npm install
# Cấu hình file .env (xem .env.example)
node server.js
```

### 4.2. Frontend

```bash
cd frontend
npm install
npm run dev
```

Truy cập `http://localhost:3000` để vào Dashboard.

## 5. Deployment (Miễn Phí)

### Frontend (Vercel)
1.  Push code `frontend` lên GitHub.
2.  Vào [Vercel](https://vercel.com), chọn "New Project" -> Import repo.
3.  Vercel tự động nhận diện Next.js và deploy.

### Backend (Render / Railway)
1.  Push code `backend` lên GitHub.
2.  Vào [Render](https://render.com) hoặc [Railway](https://railway.app).
3.  Tạo "Web Service" -> Kết nối repo.
4.  Cài đặt Environment Variables (MQTT_HOST, AWS_ACCESS_KEY, ...).

### Storage (AWS S3)
*   Đăng ký AWS Free Tier (5GB S3 miễn phí 1 năm).
*   Tạo Bucket, cấp quyền Public Read cho file upload (hoặc dùng Presigned URL).

### MQTT Broker
*   Sử dụng **HiveMQ Cloud** (Free 100 connections) hoặc **EMQX Cloud** (Free Serverless) nếu bạn chưa có Broker riêng.
