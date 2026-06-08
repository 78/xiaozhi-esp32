# AOSL (Advanced Operating System Layer) - IPRO SDK Integration
# Platform abstraction layer for OS primitives
#
# USAGE: Copy this file to components/3rdparty/aosl/CMakeLists.txt
#        to integrate AOSL into IPRO SDK for IPRO7 platform.
#
# This file integrates AOSL as a component in the IPRO SDK build system.

__generate_library(1)

# Set AOSL configuration for IPRO7 platform
set(CONFIG_PLATFORM "ipro7")
set(AOSL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(AOSL_DECLARE_PROJECT OFF CACHE BOOL "Declare as Standalone Project" FORCE)

# Include the original AOSL build configuration
include(${AOSL_DIR}/cmake/build.cmake)

# Add public API includes
if(AOSL_ADD_INCLUDES_PUBLIC)
    foreach(inc_dir ${AOSL_ADD_INCLUDES_PUBLIC})
        __component_add_include_dirs(${inc_dir})
    endforeach()
endif()

# Add private includes  
if(DEFINED AOSL_ADD_INCLUDES_PRIVATE AND AOSL_ADD_INCLUDES_PRIVATE)
    list(LENGTH AOSL_ADD_INCLUDES_PRIVATE inc_count)
    if(inc_count GREATER 0)
        target_include_directories(${CURRENT_STATIC_LIBRARY} PRIVATE ${AOSL_ADD_INCLUDES_PRIVATE})
    endif()
endif()

# Add all AOSL source files
if(DEFINED AOSL_ADD_SOURCES AND AOSL_ADD_SOURCES)
    list(LENGTH AOSL_ADD_SOURCES src_count)
    if(src_count GREATER 0)
        __component_add_sources(${AOSL_ADD_SOURCES})
    endif()
endif()

# Add AOSL compiler options
if(DEFINED AOSL_ADD_OPTIONS AND AOSL_ADD_OPTIONS)
    list(LENGTH AOSL_ADD_OPTIONS opt_count)
    if(opt_count GREATER 0)
        target_compile_options(${CURRENT_STATIC_LIBRARY} PRIVATE ${AOSL_ADD_OPTIONS})
    endif()
endif()

# Add AOSL compiler definitions - use target_compile_definitions properly
if(DEFINED AOSL_ADD_DEFINITIONS AND AOSL_ADD_DEFINITIONS)
    message(STATUS "AOSL_ADD_DEFINITIONS: ${AOSL_ADD_DEFINITIONS}")
    foreach(def ${AOSL_ADD_DEFINITIONS})
        # Extract the definition without the -D prefix
        string(REGEX REPLACE "^-D" "" clean_def "${def}")
        message(STATUS "Adding definition: ${clean_def}")
        if(clean_def)
            target_compile_definitions(${CURRENT_STATIC_LIBRARY} PRIVATE "${clean_def}")
        endif()
    endforeach()
endif()
