# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "2.0.4")

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/main/boards/ai-martube-esp32s3/wifi_connect")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiaozhi)

