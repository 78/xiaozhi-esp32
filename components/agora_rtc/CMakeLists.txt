idf_component_register(INCLUDE_DIRS ${COMPONENT_PATH}/include
                       REQUIRES aosl lwip)

set(AGORA_SDK_LIB ${COMPONENT_PATH}/lib/xtensa/libagora-rtc-sdk.a)
set(AOSL_LIB ${CMAKE_BINARY_DIR}/esp-idf/aosl/libaosl.a)

# Agora RTC SDK has circular dependency with aosl.
# Use a link group to let the linker iterate until all symbols are resolved.
target_link_libraries(${COMPONENT_LIB} INTERFACE
    ${AGORA_SDK_LIB} ${AOSL_LIB} ${AGORA_SDK_LIB})
