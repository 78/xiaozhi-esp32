cmake_minimum_required(VERSION 2.4...3.5)
project(hello_rtsa)

include_directories(
    ${THIRD_PARTY}/file_parser/include
    ${UTILITY})

aux_source_directory(${UTILITY} COMMON_FILES)
add_executable(hello_rtsa hello_rtsa.c ${COMMON_FILES})
target_link_libraries(hello_rtsa agora-rtc-sdk file_parser ${LIBS})

add_executable(hello_rtsa_multi hello_rtsa_multi.c ${COMMON_FILES})
target_link_libraries(hello_rtsa_multi agora-rtc-sdk file_parser ${LIBS})
