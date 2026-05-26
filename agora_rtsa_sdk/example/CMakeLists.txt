cmake_minimum_required(VERSION 2.4...3.5)

project(example)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_SOURCE_DIR}/scripts/check.cmake)
include(${CMAKE_SOURCE_DIR}/scripts/env.cmake)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/out/${MACHINE}/)

# sdk include header path
include_directories(${CMAKE_SOURCE_DIR}/../agora_sdk/include)
# sdk library path
link_directories(${CMAKE_SOURCE_DIR}/../agora_sdk/lib/${MACHINE}
	${THIRD_PARTY}/file_parser/lib/${MACHINE}
)

# Build apps
subdirlist(SUBDIRS ${CMAKE_SOURCE_DIR})
foreach(subdir ${SUBDIRS})
  if(EXISTS ${CMAKE_SOURCE_DIR}/${subdir}/CMakeLists.txt)
    add_subdirectory(${subdir})
  endif()
endforeach()
