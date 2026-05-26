cmake_minimum_required(VERSION 2.4...3.5)
project(hello_stream_message)

include_directories(${UTILITY})
aux_source_directory(${UTILITY} COMMON_FILES)
add_executable(hello_stream_message hello_stream_message.c ${COMMON_FILES})
target_link_libraries(hello_stream_message agora-rtc-sdk ${LIBS})