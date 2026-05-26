cmake_minimum_required(VERSION 2.4...3.5)
project(hello_rtcm)

add_executable(hello_rtcm hello_rtcm.c)
target_link_libraries(hello_rtcm agora-rtc-sdk ${LIBS})
