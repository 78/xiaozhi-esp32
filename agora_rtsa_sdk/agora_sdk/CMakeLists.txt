idf_component_register(
    INCLUDE_DIRS "include"
)

target_link_libraries(${COMPONENT_LIB} INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/xtensa/libagora-rtc-sdk.a"
)
