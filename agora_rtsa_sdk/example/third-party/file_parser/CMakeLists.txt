cmake_minimum_required(VERSION 2.4...3.5)
project(file_parser)

include_directories(${PROJECT_SOURCE_DIR}/include
  # ${PROJECT_SOURCE_DIR}/../opusgroup/include
  # ${PROJECT_SOURCE_DIR}/../opusgroup/include/opus
)
link_directories(
  # ${PROJECT_SOURCE_DIR}/../opusgroup/lib/${MACHINE}
)
aux_source_directory(./src SRCS)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib/${MACHINE})

add_library(${PROJECT_NAME} STATIC ${SRCS})
# TARGET_LINK_LIBRARIES(${PROJECT_NAME} opusfile ogg opus m)
TARGET_LINK_LIBRARIES(${PROJECT_NAME})
