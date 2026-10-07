# Code Style Guide

## Formatting Tool

This project uses `clang-format` to keep the code style consistent. The `.clang-format` file in the project root is based on the Google C++ style guide with a few project-specific tweaks.

### Installing clang-format

Make sure `clang-format` is available before you use it:

- **Windows**:
  ```powershell
  winget install LLVM
  # or with Chocolatey
  choco install llvm
  ```

- **Linux**:
  ```bash
  sudo apt install clang-format              # Ubuntu/Debian
  sudo dnf install clang-tools-extra         # Fedora
  ```

- **macOS**:
  ```bash
  brew install clang-format
  ```

### Usage

1. **Format a single file**:
   ```bash
   clang-format -i path/to/your/file.cpp
   ```

2. **Format the entire project**:
   ```bash
   # Run from the project root
   find main -iname '*.h' -o -iname '*.cc' | xargs clang-format -i
   ```

3. **Check formatting without modifying files (useful in CI / pre-commit)**:
   ```bash
   clang-format --dry-run -Werror path/to/your/file.cpp
   ```

### IDE Integration

- **Visual Studio Code**:
  1. Install the C/C++ extension.
  2. Set `C_Cpp.formatting` to `clangFormat` in settings.
  3. Optionally enable `editor.formatOnSave`.

- **CLion**:
  1. Open `Editor > Code Style > C/C++` in the settings.
  2. Set `Formatter` to `clang-format`.
  3. Choose "use the .clang-format file in the project".

### Main Rules

- Indent with 4 spaces.
- Line width capped at 100 characters.
- Attach-style braces (`{` on the same line as the control statement).
- Pointers and references bind to the type (left alignment).
- Includes are sorted automatically.
- Access specifiers are indented by -4 spaces.

## C++ Runtime and Error Handling

- Project-owned C++ code targets GNU C++23. Do not require newer language features in `main/`.
- C++ exceptions and RTTI are disabled. Do not use `throw`, `try`/`catch`, `dynamic_cast`, or
  `typeid`.
- Use RAII for owned resources, including ESP-IDF handles and C APIs with explicit cleanup
  functions. Raw pointers are non-owning unless an interface documents otherwise.
- Return `std::expected<T, E>` for recoverable failures that need an error value. Prefer a small
  enum or `esp_err_t` in hot paths, and translate errors to strings at protocol or UI boundaries.
- Check an `expected` before dereferencing it. Do not call `value()` on an unchecked result because
  a bad access terminates the firmware when exceptions are disabled.
- Treat violated programmer invariants as fatal with an assertion or explicit abort; do not model
  them as recoverable runtime errors.
- Realtime audio paths use bounded queues and avoid repeated allocation. Use fixed-capacity storage
  when the maximum is known; add a pool only after profiling shows churn or fragmentation.

### Notes

1. Make sure the code has been formatted before committing.
2. Do not fix up alignment by hand after running clang-format.
3. To exclude a block from formatting, wrap it with:
   ```cpp
   // clang-format off
   your code
   // clang-format on
   ```

### FAQ

1. **Formatting fails**:
   - Check whether `clang-format` is too old.
   - Make sure the file is UTF-8 encoded.
   - Validate the syntax of your `.clang-format` file.

2. **Output differs from what you expected**:
   - Verify that the `.clang-format` in the project root is actually picked up.
   - Make sure no other `.clang-format` higher in the tree is winning.

Questions and suggestions are welcome - please open an issue or a pull request.
