# IoTForce Webapp

## 📖 Giới thiệu
Đây là mã nguồn Frontend cho dự án **IoTForce Webapp**, được xây dựng với mục tiêu cung cấp giao diện quản lý hiện đại, hiệu năng cao và dễ dàng mở rộng. Dự án sử dụng các công nghệ tiên tiến nhất trong hệ sinh thái React.

## 🛠 Tech Stack
Dự án được xây dựng dựa trên các công nghệ cốt lõi sau:

- **Core:** [React 18](https://react.dev/)
- **Language:** [TypeScript](https://www.typescriptlang.org/) (Strict mode enabled)
- **Build Tool:** [Vite 7](https://vitejs.dev/) (Cực nhanh và nhẹ)
- **Styling:**
  - [Tailwind CSS 4](https://tailwindcss.com/) (Utility-first CSS framework)
  - [HeroUI](https://www.heroui.com/) (Bộ thư viện UI components hiện đại)
  - `framer-motion` cho các hiệu ứng chuyển động.
- **State Management:**
  - [Redux Toolkit](https://redux-toolkit.js.org/) (Quản lý state toàn cục)
  - [Redux Saga](https://redux-saga.js.org/) (Quản lý side-effects, async actions)
- **Routing:** [React Router DOM v7](https://reactrouter.com/)
- **Forms:** [React Hook Form](https://react-hook-form.com/) (Xử lý form hiệu năng cao)
- **Networking:** [Axios](https://axios-http.com/)
- **Internationalization:** [i18next](https://www.i18next.com/) & `react-i18next`
- **Linting & Formatting:** ESLint 9, Prettier, Husky, Lint-staged, Commitlint.
- **Package Manager:** Yarn 4

## 📂 Cấu trúc dự án
Mã nguồn được tổ chức trong thư mục `src` với cấu trúc rõ ràng:

```
src/
├── components/   # Các UI component tái sử dụng (Button, Input, Modal, v.v.)
│   └── ...
├── config/       # Các file cấu hình hệ thống (env, constants)
├── hooks/        # Custom React Hooks
├── layouts/      # Các layout chính của ứng dụng (MainLayout, AuthLayout)
├── locales/      # File ngôn ngữ (i18n)
├── pages/        # Các trang màn hình chính (Route components)
├── providers/    # Global Context Providers
├── services/     # Xử lý gọi API và tích hợp backend
├── store/        # Cấu hình Redux (Slices, Sagas, Selectors)
├── styles/       # Global CSS/SCSS
├── types/        # TypeScript type definitions
└── utils/        # Các hàm tiện ích (Helpers)
```

## 🚀 Hướng dẫn cài đặt & Phát triển

### Yêu cầu tiên quyết
- **Node.js**: Phiên bản `>=20.19.0` (Khuyên dùng LTS)
- **Yarn**: Phiên bản 4.x (Dự án sử dụng Yarn Berry)

### Các bước cài đặt

1. **Clone repository:**
   ```bash
   git clone <repository-url>
   cd webapp
   ```

2. **Cài đặt dependencies:**
   Dự án sử dụng `yarn` để quản lý gói.
   ```bash
   yarn install
   ```
   *Lưu ý: Nếu bạn chưa có yarn, hãy chạy `corepack enable`.*

3. **Chạy môi trường phát triển (Development):**
   ```bash
   yarn dev
   ```
   Ứng dụng sẽ chạy tại `http://localhost:5173`.

### Các lệnh Scripts khác

- `yarn build`: Build production (sẽ chạy Type check trước).
- `yarn build:prod`: Build cho môi trường Production.
- `yarn build:stage`: Build cho môi trường Staging.
- `yarn preview`: Chạy thử bản build production tại local.
- `yarn lint`: Kiểm tra và tự động sửa lỗi code style.

## 📜 Quy chuẩn & Workflow

### 1. Coding Style & Linting
Dự án áp dụng nghiêm ngặt các quy chuẩn code thông qua **ESLint** và **Prettier**.
- Code sẽ được tự động kiểm tra khi bạn commit (thông qua `husky` và `lint-staged`).
- **Khuyến nghị:** Cài đặt extension ESLint và Prettier trong VS Code để tự động format khi save.

### 2. Commit Convention
Chúng tôi sử dụng **Conventional Commits**. Mọi commit message phải tuân thủ định dạng:
```
<type>(<scope>): <subject>
```
Các loại `type` phổ biến:
- `feat`: Tính năng mới
- `fix`: Sửa lỗi
- `docs`: Tài liệu
- `style`: Format, chỉnh sửa style không ảnh hưởng logic
- `refactor`: Tái cấu trúc code
- `chore`: Thay đổi tool, config, library...

Ví dụ: `feat(auth): implement login page UI`

### 3. Absolute Imports
Sử dụng alias `@/` để import file từ thư mục `src`.
Tránh dùng: `../../components/Button`
Hãy dùng: `@/components/Button`

### 4. Git Flow
- Branch `main` (hoặc `master`): Môi trường Production ổn định.
- Branch `develop` (nếu có): Môi trường phát triển chính.
- Tạo branch mới cho mỗi `feature` hoặc `fix` từ branch chính và tạo Pull Request (PR) để merge lại.

---
*Tài liệu này được tạo tự động bởi Tech Lead AI Assistant.*
