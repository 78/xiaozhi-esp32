# 代码风格指南

## 代码格式化工具

本项目使用 clang-format 工具来统一代码风格。我们已经在项目根目录下提供了 `.clang-format` 配置文件，该配置基于 Google C++ 风格指南，并做了一些自定义调整。

### 安装 clang-format

在使用之前，请确保你已经安装了 clang-format 工具：

- **Windows**：
  ```powershell
  winget install LLVM
  # 或者使用 Chocolatey
  choco install llvm
  ```

- **Linux**：
  ```bash
  sudo apt install clang-format  # Ubuntu/Debian
  sudo dnf install clang-tools-extra  # Fedora
  ```

- **macOS**：
  ```bash
  brew install clang-format
  ```

### 使用方法

1. **格式化单个文件**：
   ```bash
   clang-format -i path/to/your/file.cpp
   ```

2. **格式化整个项目**：
   ```bash
   # 在项目根目录下执行
   find main -iname *.h -o -iname *.cc | xargs clang-format -i
   ```

3. **在提交代码前检查格式**：
   ```bash
   # 检查文件格式是否符合规范（不修改文件）
   clang-format --dry-run -Werror path/to/your/file.cpp
   ```

### IDE 集成

- **Visual Studio Code**：
  1. 安装 C/C++ 扩展
  2. 在设置中启用 `C_Cpp.formatting` 为 `clang-format`
  3. 可以设置保存时自动格式化：`editor.formatOnSave: true`

- **CLion**：
  1. 在设置中选择 `Editor > Code Style > C/C++`
  2. 将 `Formatter` 设置为 `clang-format`
  3. 选择使用项目中的 `.clang-format` 配置文件

### 主要格式规则

- 缩进使用 4 个空格
- 行宽限制为 100 字符
- 大括号采用 Attach 风格（与控制语句在同一行）
- 指针和引用符号靠左对齐
- 自动排序头文件包含
- 类访问修饰符缩进为 -4 空格

### 注意事项

1. 提交代码前请确保代码已经过格式化
2. 不要手动调整已格式化的代码对齐
3. 如果某段代码不希望被格式化，可以使用以下注释包围：
   ```cpp
   // clang-format off
   // 你的代码
   // clang-format on
   ```

### 常见问题

1. **格式化失败**：
   - 检查 clang-format 版本是否过低
   - 确认文件编码为 UTF-8
   - 验证 .clang-format 文件语法是否正确

2. **与期望格式不符**：
   - 检查是否使用了项目根目录下的 .clang-format 配置
   - 确认没有其他位置的 .clang-format 文件被优先使用

如有任何问题或建议，欢迎提出 issue 或 pull request。