# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "1.5.6")

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiaozhi)

# 项目根目录的 CMakeLists.txt
set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "项目根目录: ${PROJECT_ROOT_DIR}")


# 添加 ESP-IDF 组件路径
set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/components)
list(APPEND EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/managed_components)
