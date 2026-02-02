# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "2.0.5")

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)
if(WIN32)
    set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES ON)
    set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES ON)
    set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS ON)
    set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS ON)
    set(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES ON)
    set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_LIBRARIES ON)
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiaozhi)

