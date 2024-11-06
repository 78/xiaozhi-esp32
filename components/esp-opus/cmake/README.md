# Using CMake for the Opus Project

This guide provides instructions for using CMake to build the Opus project with various configuration options. CMake is a widely used build system generator that helps manage the build process across different platforms.

Note: Please keep in mind that software documentation can sometimes go out of date as new versions are released. It is always recommended to refer to the official CMake documentation for the most up-to-date and accurate information. You can find the official CMake documentation at [cmake.org/documentation](https://cmake.org/documentation/).

## Prerequisites

Before proceeding, make sure you have the following prerequisites installed:

- CMake
- Git (optional, but recommended for version control integration)
- Working C compiler

## Build Instructions

Follow the steps below to build the Opus project using CMake:

1. Clone the Opus repository using Git:

    ```shell
    git clone https://gitlab.xiph.org/xiph/opus
    ```

2. Create a build directory within the Opus repository:

    ```shell
    cd opus
    mkdir build
    cd build
    ```

3. Configure the build with CMake. You can set the desired configuration options using CMake's `-D` flag. Here are some available options:

   - `OPUS_BUILD_SHARED_LIBRARY`: build shared library.
   - `OPUS_BUILD_TESTING`: build tests.
   - `OPUS_BUILD_PROGRAMS`: build programs.
   - `OPUS_CUSTOM_MODES`, enable non-Opus modes, e.g. 44.1 kHz & 2^n frames.

   For example, to enable the custom modes and build programs, use the following command:

    ```shell
    cmake .. -DOPUS_BUILD_PROGRAMS=ON -DOPUS_BUILD_TESTING=ON
    ```

4. Build the Opus project:

    ```shell
    cmake --build .
    ```

5. After a successful build, you can find the compiled Opus library and associated files in the build directory.

## Testing with CTest

Opus provides a comprehensive test suite to ensure the functionality and correctness of the project. You can execute the tests using CTest, a part of the CMake build system. CTest allows for automated testing and provides useful features for managing and evaluating the test results.

To run the Opus tests using CTest, follow these steps:

1. Navigate to the build directory after configuring and building the project with CMake:

    ```shell
    cd build
    ```

2. Execute the tests using CTest:

    ```shell
    ctest
    ```

Note: For Windows you need to specify which configuration to test

```shell
ctest -C Debug
```

## Platform Support and Bug Reporting

CMake aims to provide broad platform support, allowing the Opus project to be built and used on major operating systems and platforms. The supported platforms include:

- Windows
- macOS
- Linux
- Android
- iOS

CMake achieves platform support by generating platform-specific build files (e.g., Makefiles, Visual Studio projects) based on the target platform. This allows developers to build and configure the Opus project consistently across different operating systems and environments.

While CMake strives to ensure compatibility and stability across platforms, bugs or issues may still arise in specific configurations. If you encounter any problems during the configuration process or while building the Opus project, we encourage you to file an issue in the [project's issue tracker](https://gitlab.xiph.org/xiph/opus/-/issues).

When reporting an issue, please provide the following information to help us understand and reproduce the configuration problem effectively:

1. Detailed description of the issue, including any error messages or unexpected behavior observed.
2. Steps to reproduce the problem, including the CMake command and any specific configuration options used.
3. Operating system and version (e.g., Windows 10, macOS Big Sur, Ubuntu 20.04).
4. CMake version (e.g., CMake 3.21.1).
5. Any relevant information about the platform, toolchain, or dependencies used.
6. Additional context or details that might assist in troubleshooting the issue.

By providing thorough information when reporting configuration issues, you contribute to improving the Opus project's compatibility and reliability across different platforms.

We appreciate your help in identifying and addressing any configuration-related problems, ensuring a better experience for all users of the Opus project.

## Platform Specific Examples

Note: Examples can go out of date. Always refer to documentation for latest reference.

### Cross compiling for Android

```shell
cmake .. -DCMAKE_TOOLCHAIN_FILE=${ANDROID_HOME}/ndk/25.2.9519653/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a
```

For more information about cross compiling for android, you can refer to the [Cross compiling for Android documentation](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-android).

### Cross compiling for iOS

```shell
cmake .. -G "Unix Makefiles" -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64
```

For more information about cross compilation for iOS, you can refer to the [Cross compiling for iOS documentation](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-ios-tvos-or-watchos).


### Windows Visual Studio

```shell
cmake .. -G "Visual Studio 17 2022" -A x64
```

For more information about the Visual Studio generator options and additional customization, you can refer to the [Visual Studio Generator documentation](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2017%202022.html).
