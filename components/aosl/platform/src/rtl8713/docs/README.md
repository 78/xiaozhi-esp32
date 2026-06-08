# AOSL RTL8713 Platform Integration Guide

This document describes how to integrate AOSL (Agile Operating System Layer) into the RTL8713 development environment and how to run AOSL unit tests.

## Table of Contents

- [Integration Steps](#integration-steps)
- [Running Unit Tests](#running-unit-tests)
- [Important Notes](#important-notes)
- [Directory Structure](#directory-structure)

## Integration Steps

The following steps will guide you through integrating the AOSL project into the RTL8713 ameba-rtos development environment. AOSL will be integrated as a component in the ameba-rtos project.

### Prerequisites

- You have obtained the ameba-rtos project through RTL official documentation
- The RTL8713 development environment is properly configured

### Integration Steps

1. **Create Component Directory**
   
   Create an `aosl` directory in the `component` directory.

2. **Clone AOSL Project**
   
   Clone the AOSL open-source project into the `component/aosl` directory.

3. **Copy CMakeLists.txt File**
   
   Copy the `CMakeLists.txt` file from `platform/src/rtl8713/docs/` to the `component/aosl` directory.

4. **Verify Directory Structure**
   
   After completing the above steps, your directory structure should look like this:

   ```
   ameba-rtos/
   ├── component/
   │   ├── aosl/
   │   │   ├── aosl/          # AOSL source code directory
   │   │   └── CMakeLists.txt # Build file copied from docs directory
   ```

5. **Build the Project**
   
   Follow the standard build process for ameba-rtos. AOSL will be automatically integrated as a component.

## Running Unit Tests

AOSL provides unit test functionality to help you verify that AOSL works correctly on the RTL8713 platform.

### Test Implementation Location

AOSL unit tests are implemented in the `test/aosl_test.c` file.

### Running Test Steps

1. **Create Example Program**
   
   Create an example program following the RTL official documentation.

2. **Include Test Header File**
   
   Include the `aosl_test.h` header file in your example program:
   
   ```c
   #include <api/aosl_test.h>
   ```

3. **Call Test Function**
   
   Call the `aosl_test()` function at an appropriate location to execute the unit tests.

### Important Note

**WiFi Connection Requirement**: Before calling the `aosl_test()` function, you must ensure that WiFi is properly connected.

In the RTL8713 project, the `LwIP_Check_Connectivity()` function is typically used to wait for a successful WiFi connection. Therefore, it is recommended to call `LwIP_Check_Connectivity()` before calling `aosl_test()` to ensure that the network connection is established.

Example code:

```c
#include <api/aosl_test.h>

// Wait for WiFi connection
LwIP_Check_Connectivity();

// Run AOSL unit tests
aosl_test();
```

## Important Notes

1. **Platform Configuration**: Ensure that `CONFIG_PLATFORM` in `CMakeLists.txt` is set to `"rtl8713"`.

2. **Build Options**: The `CMakeLists.txt` file has been configured with build options suitable for ameba-rtos, including:
   - `AOSL_DECLARE_PROJECT OFF`: Not declared as a standalone project

3. **Network Dependency**: Some AOSL features depend on network connectivity. Please ensure that WiFi is properly configured and connected before running tests.

## Directory Structure

AOSL RTL8713 platform implementation files are located in the `platform/src/rtl8713/` directory, mainly including the following HAL (Hardware Abstraction Layer) implementations:

- `aosl_hal_atomic.c` - Atomic operations implementation
- `aosl_hal_errno.c` - Error code mapping
- `aosl_hal_file.c` - File operations implementation
- `aosl_hal_iomp.c` - I/O multiplexing implementation
- `aosl_hal_log.c` - Logging output implementation
- `aosl_hal_memory.c` - Memory management implementation
- `aosl_hal_socket.c` - Network socket implementation
- `aosl_hal_thread.c` - Thread management implementation
- `aosl_hal_time.c` - Time-related implementation
- `aosl_hal_utils.c` - Utility functions implementation

---

**Version Information**: This document applies to AOSL RTL8713 platform integration.
