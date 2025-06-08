# Development Guide

This document provides guidelines and information for developers working on the Xiaozhi AI Chatbot project. Its goal is to help you understand the project structure, build process, and how to contribute effectively.

The Xiaozhi AI Chatbot is an ESP32-based project that utilizes the MCP (Multi-Chip Protocol) for communication, enabling AI-driven voice interactions with various hardware components and services. For a general overview of the project, its features, and supported hardware, please refer to the main [README.md](README.md).

## Prerequisites

Before you begin development, ensure you have the following software and tools installed:

*   **ESP-IDF (Espressif IoT Development Framework):** Version 5.4 or newer is recommended. This is the core SDK for ESP32 development. You can find the installation guide [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).
*   **CMake:** Used by ESP-IDF for building the project. It's typically installed as part of the ESP-IDF setup.
*   **Python:** Required for various helper scripts used in the project and by ESP-IDF.
*   **Git:** For version control and cloning the repository.

**Recommended Development Environment:**

*   **Operating System:** Linux is highly recommended for a smoother development experience, faster compilation times, and fewer driver-related issues. Windows and macOS are also supported by ESP-IDF.
*   **Code Editor:** A modern code editor such as VSCode (Visual Studio Code) or Cursor, preferably with the official Espressif IDF Plugin, will provide a better development experience with features like code completion, debugging, and integrated terminal.

## Project Structure

Understanding the project's directory structure is key to navigating and contributing to the codebase.

*   `main/`: This is where the core application logic resides.
    *   `main/application.cc/h`: Manages the overall application state and flow.
    *   `main/boards/`: Contains board-specific configuration and initialization files. Each supported hardware board has its own subdirectory here (e.g., `main/boards/esp-box/`).
    *   `main/audio_codecs/`: Drivers for various audio codec chips (e.g., ES8311, ES8374).
    *   `main/audio_processing/`: Modules for audio processing tasks like wake-word detection and voice activity detection.
    *   `main/display/`: Drivers and rendering logic for different display types (e.g., LCD, OLED).
    *   `main/led/`: Code for controlling LEDs, including status indicators and decorative strips.
    *   `main/iot/`: Implementation of "Things" that the AI can control via the MCP protocol (e.g., speaker volume, screen brightness).
    *   `main/protocols/`: Implementation of communication protocols like WebSocket and MQTT.
    *   `main/CMakeLists.txt`: CMake file specific to the `main` component, linking sources and managing dependencies within `main`.
*   `docs/`: Contains project documentation, including user guides, protocol specifications, and hardware information.
*   `scripts/`: Includes various Python helper scripts for tasks like building firmware for specific boards (`release.py`), flashing (`flash.sh` - example), converting assets, etc.
*   `partitions/`: Contains partition table CSV files (`.csv`) that define how the flash memory is organized for different configurations (e.g., based on flash size).
*   `CMakeLists.txt` (root): The main CMake file for the project. It sets up the project, includes ESP-IDF's build system, and defines project-wide settings.
*   `sdkconfig.defaults`: Provides default Kconfig values for the project. Board-specific configurations often override or extend these.
*   `.github/`: Contains GitHub-specific files, such as issue templates (`ISSUE_TEMPLATE/`) and workflow definitions (`workflows/`) for continuous integration.

## Build Process

The primary method for building firmware for specific hardware targets is by using the `scripts/release.py` script. This script automates the process of configuring the project for a particular board, building the firmware, and packaging it.

**Building with `release.py`:**

*   **List available board types:** The script can usually infer board types or you can inspect `main/boards/` directory names and `main/CMakeLists.txt` for `CONFIG_BOARD_TYPE_` definitions.
*   **Build for a specific board:**
    ```bash
    python scripts/release.py <board_directory_name>
    ```
    Replace `<board_directory_name>` with the actual name of the board's directory in `main/boards/` (e.g., `esp-box`, `lichuang-c3-dev`).
*   **Build for all supported boards:**
    ```bash
    python scripts/release.py all
    ```
    This will iterate through all configured boards, build the firmware for each, and package them. This can take a significant amount of time.

The `release.py` script handles:
1.  Reading the board's `config.json` file (`main/boards/<board_directory_name>/config.json`).
2.  Setting the correct ESP-IDF target (e.g., `esp32s3`, `esp32c3`) using `idf.py set-target`.
3.  Temporarily appending board-specific SDK configurations to the project's `sdkconfig` file.
4.  Executing the build using `idf.py build`.
5.  Merging the binaries using `idf.py merge-bin`.
6.  Packaging the final `merged-binary.bin` into a `.zip` file in the `releases/` directory, named like `v<project_version>_<board_name>.zip`.

**Incremental Builds (Standard ESP-IDF):**

For day-to-day development on a specific, already configured target, you can use the standard ESP-IDF build commands:

1.  **Set your desired target (if not already set or if switching targets):**
    ```bash
    idf.py set-target <target_chip>
    ```
    (e.g., `idf.py set-target esp32s3`)
2.  **Configure the project (optional, if you need to change specific options):**
    ```bash
    idf.py menuconfig
    ```
3.  **Build the project:**
    ```bash
    idf.py build
    ```
This will perform an incremental build, which is faster if only a few files have changed. The output binaries will be located in the `build/` directory.

## Flashing Firmware

Once the firmware is built, you need to flash it to your ESP32 device.

**Using ESP-IDF `flash` command:**

The most common way to flash the firmware after a build is using the `idf.py flash` command. This command automatically detects the serial port (on Linux and macOS, usually) and flashes the binaries created by the last `idf.py build` or by the `scripts/release.py` script (as it calls `idf.py build` internally).

1.  Ensure your ESP32 device is connected to your computer via USB.
2.  Run the command:
    ```bash
    idf.py flash
    ```
    You might need to specify the serial port if it's not detected automatically or if you have multiple devices connected:
    ```bash
    idf.py -p /dev/ttyUSB0 flash
    ```
    (Replace `/dev/ttyUSB0` with your actual serial port. On Windows, it will be something like `COM3`).

**Using `esptool.py` directly:**

For more advanced control or specific flashing scenarios, you can use `esptool.py` directly. This is the underlying tool used by ESP-IDF. The `scripts/flash.sh` script in the repository provides a basic example:

```bash
#!/bin/sh
# Example: scripts/flash.sh
# esptool.py -p /dev/ttyACM0 -b 2000000 write_flash 0 ../releases/v0.9.9_bread-compact-wifi/merged-binary.bin
```
This script is hardcoded for a specific port, baud rate, and binary file. You would need to adapt it for your needs, pointing to the correct `merged-binary.bin` (usually found in `build/` after a build, or from a `.zip` file in `releases/`).

Refer to the `esptool.py` documentation for its full capabilities.

## Adding a New Board

This project supports a wide variety of ESP32-based hardware boards. If you want to add support for a new, custom board, follow these steps. For a more detailed guide, refer to [main/boards/README.md](main/boards/README.md).

**1. Create Board-Specific Files:**

*   **Create a new directory:** Inside `main/boards/`, create a directory for your new board (e.g., `main/boards/my-custom-board/`).
*   **`config.h`:** In your new board's directory, create a `config.h` file. This file defines hardware pin mappings (e.g., for I2S, I2C, SPI, buttons, LEDs), audio sampling rates, display parameters, and other board-specific hardware configurations. Refer to existing `config.h` files in other board directories for examples.
*   **`config.json`:** Create a `config.json` file. This JSON file specifies:
    *   `target`: The ESP32 chip type (e.g., "esp32s3", "esp32c3").
    *   `builds`: An array of build configurations. Each build should have a unique `name` (which becomes part of the firmware filename) and can include `sdkconfig_append` to add or override specific Kconfig options from `sdkconfig.defaults` or the ESP-IDF defaults.
        ```json
        {
            "target": "esp32s3",
            "builds": [
                {
                    "name": "my-custom-board-v1", // Unique name for this build variant
                    "sdkconfig_append": [
                        "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y",
                        "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v1/8m.csv\""
                    ]
                }
            ]
        }
        ```
    *   **Important:** The `name` in `config.json` should be unique across all boards and build variants to ensure OTA (Over-The-Air) updates work correctly and don't inadvertently send firmware for one board type to another. It's good practice to prefix it with your board's directory name.
*   **`<board_name>.cc`:** Create the C++ source file for your board (e.g., `my_custom_board.cc`). This file contains the board initialization class.
    *   The class should inherit from a base board class like `WifiBoard` (for Wi-Fi enabled boards), `Ml307Board` (for 4G Cat.1 boards using ML307), or `DualNetworkBoard`.
    *   Implement necessary virtual methods like `GetAudioCodec()`, `GetDisplay()`, `GetBacklight()`, `InitializeButtons()`, etc., to provide instances of drivers and initialize hardware components specific to your board.
    *   Use the `DECLARE_BOARD(<YourBoardClassName>);` macro at the end of the file to register your board.

**2. Integrate into the Build System:**

To make the build system aware of your new board, you need to modify `main/CMakeLists.txt`:

*   **Add a Kconfig-like option:** Find the section with `if(CONFIG_BOARD_TYPE_...)` statements. Add a new one for your board:
    ```cmake
    # Example for my-custom-board
    if(CONFIG_BOARD_TYPE_MY_CUSTOM_BOARD)
        set(BOARD_TYPE "my-custom-board")
        set(BOARD_KCONFIG_NAME "MY_CUSTOM_BOARD") # Used for Kconfig choice
        add_subdirectory(boards/${BOARD_TYPE})
    endif()
    ```
*   **Update the Kconfig choice:** A little further down in `main/CMakeLists.txt`, you'll find `idf_component_get_property(mc Kconfig)`. After this, there's a `set(kconfig_content ...)`. You need to add your board to the `choice` list and define its `config` option.
    *   Add to `choice`:
        ```
        choice BOARD_TYPE_CHOICE ...
            config BOARD_TYPE_MY_CUSTOM_BOARD # Add this line
            ...
        endchoice
        ```
    *   Add the config definition:
        ```cmake
        config BOARD_TYPE_MY_CUSTOM_BOARD
            bool "My Custom Board" # This is the name that will appear in menuconfig
        ```

    This makes your board selectable in `idf.py menuconfig` under "Board Configuration". The `BOARD_TYPE` variable set earlier is used by `scripts/release.py` to find your board's directory.

**3. Build and Test:**

*   Build the firmware for your new board using `python scripts/release.py my-custom-board` (or whatever you named its directory).
*   Flash and test thoroughly.

Refer to existing board implementations in `main/boards/` for practical examples of how `config.h`, `config.json`, and the C++ board files are structured.

## Coding Style

This project adheres to the **Google C++ Style Guide**. Consistency in coding style is important for readability and maintainability of the codebase.

*   Please familiarize yourself with the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
*   Before committing your changes, ensure your code is formatted according to these guidelines.
*   Using a C++ linter and formatter (like `clang-format` configured for Google style) is highly recommended to automate this process.

Adhering to the coding style helps everyone understand the code more easily and makes code reviews more efficient.

## Debugging

Effective debugging is crucial for firmware development. Here are some common techniques and tools used in this project:

**1. ESP-IDF Monitor:**

The `idf.py monitor` command is one of the most useful tools for debugging. It displays serial output from your ESP32 device and can also be used to send input to it.

*   Run after flashing:
    ```bash
    idf.py monitor
    ```
*   If the monitor doesn't start automatically after flashing, you can run it separately.
*   Use `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`, etc., macros in your C++ code to print debug messages. These messages will appear in the monitor.
*   The monitor also provides useful features like resetting the device, and decoding stack traces in case of a crash.

**2. Logging:**

Leverage the ESP-IDF logging library (`esp_log.h`).
*   Use different log levels (Verbose, Debug, Info, Warning, Error) to control the amount of output.
*   You can configure log levels for specific components at runtime or compile time (via `menuconfig`).

**3. JTAG Debugging:**

For more advanced debugging, ESP-IDF supports JTAG debugging with tools like OpenOCD and GDB. This allows you to set breakpoints, step through code, inspect variables, and more. Refer to the [ESP-IDF JTAG Debugging Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/jtag-debugging/index.html) for setup and usage instructions.

**4. Audio Debugging Server:**

The project includes a script `scripts/audio_debug_server.py`. This script can be used to receive and analyze audio data streamed from the device during development, which is particularly useful when working on audio input, processing, or wake-word features. Check the script's contents or any accompanying documentation for specific usage instructions.

**5. Unit Testing (if applicable):**

While not explicitly detailed here, writing and running unit tests for individual modules can help catch bugs early. ESP-IDF has support for unit testing with the Unity framework.
