idf_component_register(INCLUDE_DIRS ${COMPONENT_PATH}/include
                       REQUIRES aosl lwip)

# Link the prebuilt Agora RTC static library
target_link_libraries(${COMPONENT_LIB} INTERFACE ${COMPONENT_PATH}/lib/xtensa/libagora-rtc-sdk.a)
