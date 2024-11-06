# Using the Meson Build System for the Opus Project

This guide provides instructions for using the Meson build system to build the Opus project with various configuration options. Meson is a fast and efficient build system that aims to be easy to use and understand.

Please note that software documentation can sometimes become outdated as new versions are released. For the most up-to-date and accurate information, we recommend referring to the official Meson documentation, which can be found at [mesonbuild.com](https://mesonbuild.com/).

## Prerequisites

Before proceeding, ensure that you have the following prerequisites installed:

- [Meson](https://mesonbuild.com/Quick-guide.html)
- [Ninja](https://ninja-build.org/) (recommended as the build backend, although other backends are also available)
- [Git](https://git-scm.com/) (optional, but recommended for version control integration)
- A working C compiler

## Build and Test Instructions

Follow the steps below to build the Opus project using Meson.

### Check out Source
Clone the Opus repository using Git:

```shell
git clone https://gitlab.xiph.org/xiph/opus
cd opus
```

### Configure
To configure the build with Meson, you can set the desired configuration options using the -D flag followed by the option name and value. For the Opus project-specific build options, please refer to the [meson_options.txt](./../meson_options.txt) file. For general Meson options use the command meson `setup --help` to get a list of these options.

For example, to setup and disable tests, use the following command:

```shell
meson setup builddir -Dtests=disabled
```

### Build

```shell
meson compile -C builddir
```

After a successful build, you can find the compiled Opus library and associated files in the builddir directory.

### Testing with Meson

Opus provides a comprehensive test suite to ensure the functionality and correctness of the project. You can execute the tests using Meson's built-in testing functionality.

To run the Opus tests using Meson:

```shell
meson test -C builddir
```

## Platform Support and Bug Reporting

The Opus Meson build system aims to provide support for the same platforms as [GStreamer](https://gstreamer.freedesktop.org/), a widely used multimedia framework. GStreamer supports a wide range of operating systems and platforms, including Linux, Windows (MSVC and MingW), Android, macOS, iOS, and various BSD systems.

While efforts are made to ensure compatibility and stability across these platforms, bugs or issues may still arise in specific configurations. If you encounter any problems during the configuration process or while building the Opus project, we encourage you to file an issue in the [project's issue tracker](https://gitlab.xiph.org/xiph/opus/-/issues).

When reporting an issue, please provide the following information to help us understand and effectively reproduce the configuration problem:

1. Detailed description of the issue, including any error messages or unexpected behavior observed.
2. Steps to reproduce the problem, including the Meson command and any specific configuration options used.
3. Operating system and version (e.g., Windows 10, macOS Big Sur, Ubuntu 20.04).
4. Meson version (e.g., Meson 0.60.0).
5. Any relevant information about the platform, toolchain, or dependencies used.
6. Additional context or details that might assist in troubleshooting the issue.

By providing thorough information when reporting configuration issues, you contribute to improving the Opus project's compatibility and reliability across different platforms.

We appreciate your help in identifying and addressing any configuration-related problems, ensuring a better experience for all users of the Opus project.

## Platform Specific Examples

Note: Examples can become outdated. Always refer to the documentation for the latest reference.

### Windows Visual Studio

To generate Visual Studio projects, Meson needs to know the settings of your installed version of Visual Studio. The recommended approach is to run Meson under the Visual Studio Command Prompt.

You can find the Visual Studio Command Prompt by searching from the Start Menu. However, the name varies for each Visual Studio version. For Visual Studio 2022, look for "x64 Native Tools Command Prompt for VS 2022". The following steps remain the same:

```shell
meson setup builddir -Dtests=enabled --backend vs
```

For more information about the Visual Studio backend options and additional customization, please refer to the [Using with Visual Studio](https://mesonbuild.com/Using-with-Visual-Studio.html) documentation.