# AOSL IPRO7 Platform Integration Guide

This document describes how to integrate AOSL (Advanced Operating System Layer) into the IPRO SDK development environment and how to run AOSL unit tests.

## Table of Contents

- [Integration Steps](#integration-steps)
- [Running Unit Tests](#running-unit-tests)
- [Important Notes](#important-notes)
- [Directory Structure](#directory-structure)

## Integration Steps

The following steps will guide you through integrating the AOSL project into the IPRO SDK development environment. AOSL will be integrated as a component in the IPRO SDK project.

### Prerequisites

- You have obtained the IPRO SDK project
- The IPRO7 development environment is properly configured
- FreeRTOS toolchain is installed at `/opt/toolchain/riscv_ipro7/bin/`

### Integration Steps

AOSL is already integrated into the IPRO SDK as a third-party component. The integration structure is as follows:

1. **Component Location**
   
   AOSL is located in the `components/3rdparty/aosl/` directory.

2. **Platform-Specific Implementation**
   
   IPRO7 platform HAL implementations are in `components/3rdparty/aosl/platform/src/ipro7/`.

3. **CMakeLists.txt Integration** (Optional - for custom integration)
   
   If you need to customize the AOSL integration, you can copy the reference CMakeLists.txt:
   
   ```bash
   # Backup original (if needed)
   cp components/3rdparty/aosl/CMakeLists.txt components/3rdparty/aosl/CMakeLists.txt.bak
   
   # Copy the IPRO7-specific integration file
   cp components/3rdparty/aosl/platform/src/ipro7/docs/CMakeLists.txt \
      components/3rdparty/aosl/CMakeLists.txt
   ```
   
   **Note**: The default CMakeLists.txt already works for IPRO7. Only copy if you need customization.

4. **Verify Directory Structure**
   
   Your directory structure should look like this:

   ```
   ipro_sdk/
   ├── components/
   │   ├── 3rdparty/
   │   │   ├── aosl/
   │   │   │   ├── aosl/          # AOSL source code directory
   │   │   │   ├── platform/      # Platform-specific implementations
   │   │   │   │   └── src/
   │   │   │   │       └── ipro7/ # IPRO7 HAL implementation
   │   │   │   └── CMakeLists.txt # Build file for IPRO SDK
   ```

4. **Build the Project**
   
   Follow the standard build process for IPRO SDK. AOSL will be automatically integrated as a component:
   
   ```bash
   # Build with FreeRTOS (recommended)
   ./build_freertos.sh <PROJECT_NAME> build
   
   # Example: Build ipro7_demo with AOSL
   ./build_freertos.sh ipro7_demo build
   ```

## Running Unit Tests

AOSL provides unit test functionality to help you verify that AOSL works correctly on the IPRO7 platform.

### Test Implementation Location

AOSL unit tests are implemented in the `test/aosl_test.c` file.

### Running Test Steps

1. **Enable AOSL Test in Application**
   
   In your application (e.g., `apps/ipro_aosl_test/`), enable AOSL component in the `.config` file:
   
   ```kconfig
   CONFIG_USE_AOSL=y
   ```

2. **Include Test Header File**
   
   Include the `aosl_test.h` header file in your application:
   
   ```c
   #include <api/aosl_test.h>
   ```

3. **Call Test Function**
   
   Call the `aosl_test()` function in your application's main task to execute the unit tests:
   
   ```c
   #include <api/aosl_test.h>
   
   void app_main(void)
   {
       // Initialize system components
       
       // Wait for network connection (if testing network features)
       // lwip_check_connectivity();
       
       // Run AOSL unit tests
       aosl_test();
   }
   ```

### Important Note

**Network Dependency**: Some AOSL features (especially socket-related tests) depend on network connectivity. If testing network functionality:

1. Ensure lwIP stack is properly initialized
2. Configure and establish WiFi/Ethernet connection before running tests
3. Wait for network to be ready using appropriate connection check functions

Example code for network-dependent tests:

```c
#include <api/aosl_test.h>
#include <lwip/netif.h>

void app_main(void)
{
    // Wait for network interface to be up
    while (!netif_is_link_up(netif_default)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Run AOSL unit tests
    aosl_test();
}
```

## Important Notes

1. **Platform Configuration**: The platform is automatically configured to `"ipro7"` in the AOSL CMakeLists.txt.

2. **Build Options**: The CMakeLists.txt file has been configured with build options suitable for IPRO SDK, including:
   - `AOSL_DECLARE_PROJECT OFF`: Not declared as a standalone project
   - Platform-specific HAL implementations for IPRO7/FreeRTOS

3. **FreeRTOS Integration**: AOSL HAL layer is implemented using FreeRTOS primitives:
   - Thread management uses FreeRTOS tasks
   - Synchronization uses FreeRTOS semaphores and mutexes
   - Memory allocation uses FreeRTOS heap management
   - Time functions use FreeRTOS tick count

4. **Network Stack**: Socket operations are implemented using lwIP network stack.

5. **Test Application**: A dedicated test application `apps/ipro_aosl_test/` is available for comprehensive AOSL testing.

## Directory Structure

AOSL IPRO7 platform implementation files are located in the `platform/src/ipro7/` directory, mainly including the following HAL (Hardware Abstraction Layer) implementations:

- `aosl_hal_atomic.c` - Atomic operations implementation (FreeRTOS-based)
- `aosl_hal_errno.c` - Error code mapping for IPRO7/FreeRTOS
- `aosl_hal_file.c` - File operations implementation (POSIX-style)
- `aosl_hal_iomp.c` - I/O multiplexing implementation (select/poll)
- `aosl_hal_log.c` - Logging output implementation (IPRO_LOG integration)
- `aosl_hal_memory.c` - Memory management implementation (FreeRTOS heap)
- `aosl_hal_socket.c` - Network socket implementation (lwIP-based)
- `aosl_hal_thread.c` - Thread management implementation (FreeRTOS tasks)
- `aosl_hal_time.c` - Time-related implementation (FreeRTOS tick-based)
- `aosl_hal_utils.c` - Utility functions implementation

### Configuration Files

- `config/hal/` - Platform-specific HAL configuration headers
- `docs/CMakeLists.txt` - **IPRO7 integration template** (can be copied to `components/3rdparty/aosl/CMakeLists.txt` for customization)

## Build System Integration

AOSL integrates with IPRO SDK's CMake-based build system:

1. **Component Registration**: AOSL is registered as a component in `components/3rdparty/aosl/CMakeLists.txt`

2. **Platform Selection**: Platform is selected via `CONFIG_PLATFORM` variable (set to "ipro7")

3. **Automatic Source Discovery**: CMake automatically discovers and compiles platform-specific HAL implementations

4. **Dependency Management**: AOSL automatically links with required IPRO SDK components:
   - FreeRTOS kernel
   - lwIP network stack
   - System utilities

## Troubleshooting

### Build Issues

- **Toolchain not found**: Ensure RISC-V toolchain is installed at `/opt/toolchain/riscv_ipro7/bin/`
- **Missing dependencies**: Check that FreeRTOS and lwIP components are enabled in `.config`

### Runtime Issues

- **Socket tests fail**: Verify network connectivity and lwIP initialization
- **Thread tests fail**: Check FreeRTOS heap size configuration
- **Time tests fail**: Ensure FreeRTOS tick rate is properly configured

### Debug Tips

- Enable AOSL debug logging by defining appropriate log level macros
- Check FreeRTOS task stack sizes if experiencing stack overflow
- Monitor heap usage during tests to detect memory leaks
- Use IPRO_LOG macros to trace AOSL operations

---

**Version Information**: This document applies to AOSL IPRO7 platform integration with IPRO SDK.

**Last Updated**: February 5, 2026
