idf_component_register(
    SRCS "src/audio_malloc.c"
    INCLUDE_DIRS "include"
)

target_link_libraries(${COMPONENT_LIB} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/libs/${CONFIG_IDF_TARGET}/libesp_codec.a")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u audio_free -u audio_calloc_inner -u audio_calloc")