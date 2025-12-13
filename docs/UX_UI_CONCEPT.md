# UI/UX Design System & Concept

Tài liệu này mô tả chi tiết hệ thống thiết kế (Design System) của dự án, bao gồm màu sắc, typography, bố cục và phong cách thành phần. Sử dụng tài liệu này để đảm bảo tính đồng bộ khi phát triển các hệ thống vệ tinh hoặc mở rộng tính năng.

## 1. Triết lý thiết kế (Design Philosophy)
Hệ thống hướng tới phong cách **Modern, Clean & Soft** (Hiện đại, Sạch sẽ & Mềm mại).
- **Màu sắc**: Sử dụng tông màu tím (Violet) làm chủ đạo, tạo cảm giác công nghệ nhưng nhẹ nhàng, không gay gắt.
- **Hình khối**: Bo góc lớn (Rounded), đổ bóng sâu (Deep Shadows) tạo chiều sâu không gian.
- **Tương phản**: Hỗ trợ tốt cả Light Mode và Dark Mode với độ tương phản được tinh chỉnh kỹ lưỡng (Off-white thay vì Pure white).

---

## 2. Bảng màu (Color Palette)

Hệ thống sử dụng Tailwind CSS với bảng màu được tùy biến (`tailwind.config.js`).

### Primary Color (Màu chủ đạo - Violet)
Dùng cho các nút hành động chính (CTA), trạng thái active, và điểm nhấn thương hiệu.
- **DEFAULT**: `#8B6FB5` (Màu tím trung tính, dịu mắt)
- **Light (50-200)**: `#F5F3F8` (Nền nhẹ), `#DDD6E8`
- **Dark (700-900)**: `#6B4F8A`, `#4A2F5C` (Dùng cho text trên nền sáng)

### Secondary / Neutral (Màu trung tính - Slate/Gray)
Dùng cho văn bản, đường viền, và nền phụ.
- **DEFAULT**: `#6B7280`
- **Background Light**: `#FAFAF9` (Warm Off-white - Trắng ngà ấm, không phải trắng tinh #FFFFFF)
- **Background Dark**: `#1A1A1A` (Deep Charcoal - Đen than, không phải đen tuyền #000000)
- **Foreground (Text)**: `#FFFFFF` (Trên nền tối) / `#111827` (Trên nền sáng)

### Semantic Colors (Màu ngữ nghĩa)
- **Success (Thành công)**: `#34D399` (Xanh ngọc)
- **Warning (Cảnh báo)**: `#FBBF24` (Vàng hổ phách)
- **Danger (Nguy hiểm/Lỗi)**: `#F43F5E` (Đỏ hồng)

---

## 3. Typography (Kiểu chữ & Tỷ lệ)

### Font Family
Sử dụng System Font Stack (San Francisco, Segoe UI, Roboto, Inter) để tối ưu hiệu năng và độ rõ nét trên từng hệ điều hành.

### Text Styles
- **Headings (Tiêu đề)**:
  - Font weight: `Bold` hoặc `Semibold`.
  - Letter spacing: `tracking-tight` (Khoảng cách chữ hẹp để tạo khối vững chắc).
  - Gradient Text: Các tiêu đề lớn thường sử dụng hiệu ứng gradient dọc.
    - Ví dụ Violet: `from-[#A78BC8] to-[#8B6FB5]`
- **Body (Nội dung)**:
  - Size: `text-base` (16px) hoặc `text-sm` (14px).
  - Color: `text-default-600` (Màu xám trung tính, không dùng đen tuyền để tránh mỏi mắt).

### Kích thước (Scale)
- **Display**: `text-4xl` đến `text-6xl` (36px - 60px)
- **H1/Title**: `text-3xl` (30px)
- **Subtitle**: `text-lg` đến `text-xl` (18px - 20px)

---

## 4. Layout & Spacing (Bố cục & Khoảng cách)

### Grid & Container
- **Max Width**: `max-w-7xl` (1280px) cho nội dung chính.
- **Padding chuẩn**: `px-6 py-8` (24px ngang, 32px dọc).

### Sidebar Layout
- **Width**: `16rem` (256px) trên Desktop.
- **Behavior**:
  - Desktop: Cố định bên trái (`md:ml-64`).
  - Mobile: Ẩn/Hiện dạng Drawer hoặc thu gọn.
- **Transition**: `duration-300` (Mượt mà khi đóng mở).

### Spacing (Khoảng cách)
Sử dụng hệ thống 4px grid của Tailwind.
- **Gap nhỏ**: `gap-3` (12px) cho các item trong list.
- **Padding Card**: `p-6` (24px) hoặc `md:p-8` (32px).
- **Margin Section**: `my-6` (24px).

---

## 5. Component Styling (Phong cách thành phần)

### Cards (Thẻ nội dung)
- **Background**:
  - Light: `#FFFFFF` (Trắng tinh trên nền Off-white).
  - Dark: `#default-100` hoặc `#1A1A1A`.
- **Border Radius**: `rounded-lg` (Bo góc lớn, mềm mại).
- **Shadow (Đổ bóng)**:
  - Mặc định: `shadow-xl` (Bóng đổ rộng, tạo cảm giác nổi hẳn lên nền).
  - Hover: `shadow-2xl`.
- **Animation**: `animate-scaleIn` (Hiệu ứng phóng to nhẹ khi xuất hiện).

### Navigation (Menu)
- **Active Item**:
  - Background: `bg-primary` (Màu tím chủ đạo).
  - Text: `text-primary-foreground` (Trắng).
  - Bo góc: `rounded-lg`.
- **Inactive Item**:
  - Text: `text-default-600`.
  - Hover: `hover:bg-default-100` (Xám nhạt).
- **Icons**: Kích thước chuẩn `22px`.

### Inputs & Forms
- **Nền**: Trong suốt hoặc xám rất nhạt.
- **Border**: Mỏng, màu `#E4E7EB`.
- **Focus**: Highlight viền màu Primary (`#8B6FB5`).

### Visual Effects (Hiệu ứng thị giác)
- **Glassmorphism**: Sử dụng cho Footer hoặc Overlay.
  - `bg-default-50/50` (Độ trong suốt 50%).
  - `backdrop-blur` (nếu cần).
- **Gradients**: Sử dụng tinh tế cho Text hoặc Button Background, tránh dùng mảng lớn gây rối mắt.

---

## 6. Tổng kết cho Designer/Developer
Khi thiết kế hệ thống mới đồng bộ với hệ thống này, hãy tuân thủ:
1. **Giữ nền sạch**: Dùng màu Off-white (`#FAFAF9`), tránh dùng màu xám quá đậm hoặc trắng quá chói làm nền chính.
2. **Màu tím là điểm nhấn**: Chỉ dùng màu tím (`#8B6FB5`) cho những thứ quan trọng (Nút bấm, Link, Active State).
3. **Bo góc và Đổ bóng**: Đừng ngại dùng `rounded-lg` và `shadow-xl`, đây là đặc trưng tạo nên sự mềm mại của giao diện.
4. **Không gian thoáng**: Luôn giữ padding rộng rãi (`p-6`, `p-8`) trong các Card chứa nội dung.
