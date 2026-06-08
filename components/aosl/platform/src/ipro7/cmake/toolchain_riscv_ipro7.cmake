if(MINGW OR CYGWIN OR WIN32)
    set(TOOLCHAIN_SUFFIX ".exe")
elseif(UNIX OR APPLE)
    set(TOOLCHAIN_SUFFIX "")
endif()

# specify cross compilers and tools
SET(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc${TOOLCHAIN_SUFFIX} CACHE INTERNAL "")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
SET(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++${TOOLCHAIN_SUFFIX} CACHE INTERNAL "")
set(CMAKE_ASM_COMPILER ${CROSS_COMPILE}gcc${TOOLCHAIN_SUFFIX} CACHE INTERNAL "")
set(CMAKE_LINKER ${CROSS_COMPILE}ld${TOOLCHAIN_SUFFIX} CACHE INTERNAL "")
set(CMAKE_OBJCOPY ${CROSS_COMPILE}objcopy CACHE INTERNAL "")
set(CMAKE_OBJDUMP ${CROSS_COMPILE}objdump CACHE INTERNAL "")
set(SIZE ${CROSS_COMPILE}size CACHE INTERNAL "")
set(CMAKE_FIND_ROOT_PATH ${CROSS_COMPILE}gcc)
#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# If CPU_ID not passed via -D, read it from .config file
if("${CPU_ID}" STREQUAL "" AND DEFINED CUSTOM_CONFIG_DIR)
    if(EXISTS "${CUSTOM_CONFIG_DIR}/.config")
        file(STRINGS "${CUSTOM_CONFIG_DIR}/.config" _cpu_id_line REGEX "^CONFIG_CPU_ID=")
        if(_cpu_id_line)
            string(REGEX REPLACE "^CONFIG_CPU_ID=\"(.*)\"" "\\1" CPU_ID "${_cpu_id_line}")
            message(STATUS "[toolchain] CPU_ID read from .config: ${CPU_ID}")
        else()
            # Fallback: infer CPU_ID from CONFIG_IPRO7 or CONFIG_CHIP
            file(STRINGS "${CUSTOM_CONFIG_DIR}/.config" _chip_line REGEX "^CONFIG_IPRO7=y")
            if(_chip_line)
                set(CPU_ID "intelpro_ipro7")
                message(STATUS "[toolchain] CPU_ID inferred from CONFIG_IPRO7: ${CPU_ID}")
            endif()
        endif()
    endif()
endif()

if ("${CPU_ID}" STREQUAL "intelpro_ipro7")
# IPRO7 architecture settings based on BSP configuration
SET(MARCH "rv32imafc")
SET(MABI "ilp32f")

# --- Detect vendor-specific toolchain extensions ---
# The patched (vendor) GCC supports: -mtune=intelpro-ipro7, xxldsp/xxldspn2x extensions
# Standard GCC only supports the standard extensions + zifencei (for fence.i instruction)
# Note: Always re-detect to avoid stale cache issues in CI environments
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -march=rv32imafc_xxldsp -mtune=intelpro-ipro7 -mabi=ilp32f -x c -c /dev/null -o /dev/null
    OUTPUT_QUIET ERROR_QUIET
    RESULT_VARIABLE _vendor_test_result
)

if(_vendor_test_result EQUAL 0)
    message(STATUS "[toolchain] Vendor toolchain detected - enabling xxldsp and mtune=intelpro-ipro7")
    SET(MARCH_EXT "_zba_zbb_zbc_zbs_xxldsp")
    SET(MARCH_EXT_DSP "_zba_zbb_zbc_zbs_xxldspn2x")
    SET(MARCH_EXT_DSP_LIB "${MARCH_EXT_DSP}")
    set(_MTUNE_FLAG "-mtune=intelpro-ipro7")
else()
    message(STATUS "[toolchain] Standard toolchain detected - using standard extensions only")
    SET(MARCH_EXT "_zba_zbb_zbc_zbs_zifencei")
    SET(MARCH_EXT_DSP "${MARCH_EXT}")
    # DSP prebuilt libs don't have zifencei in filename, use base extensions
    SET(MARCH_EXT_DSP_LIB "_zba_zbb_zbc_zbs")
    set(_MTUNE_FLAG "")
endif()

# Export for SoC CMakeLists to use
set(IPRO_MARCH "${MARCH}" CACHE STRING "" FORCE)
set(IPRO_MABI "${MABI}" CACHE STRING "" FORCE)
set(IPRO_MARCH_EXT "${MARCH_EXT}" CACHE STRING "" FORCE)
set(IPRO_MARCH_EXT_DSP "${MARCH_EXT_DSP}" CACHE STRING "" FORCE)
set(IPRO_MARCH_EXT_DSP_LIB "${MARCH_EXT_DSP_LIB}" CACHE STRING "" FORCE)
set(IPRO_MTUNE_FLAG "${_MTUNE_FLAG}" CACHE STRING "" FORCE)

# Set compiler flags for IPRO7 CPU (used by IPRO7 and IPRO6)
if(_MTUNE_FLAG)
    set(_flags "-march=${MARCH}${MARCH_EXT} -mabi=${MABI} ${_MTUNE_FLAG}")
else()
    set(_flags "-march=${MARCH}${MARCH_EXT} -mabi=${MABI}")
endif()
set(CMAKE_C_FLAGS "${_flags}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${_flags}" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS "${_flags}" CACHE STRING "" FORCE)

# NOTE: Chip definition (IPRO6LE/IPRO6/IPRO7) is set in base_sdkConfig.cmake
# after config_parse(), using CONFIG_CHIP from .config
endif()
