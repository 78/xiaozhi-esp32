set(AOSL_DIR ${COMPONENT_PATH}/../../../../../..)
set(CONFIG_PLATFORM "esp32-s3")
set(AOSL_DECLARE_PROJECT OFF CACHE BOOL "Declare as Standalone Project" FORCE)

include(${AOSL_DIR}/CMakeLists.txt)

idf_component_register(SRCS ${AOSL_ADD_SOURCES}
                       INCLUDE_DIRS ${AOSL_ADD_INCLUDES_PUBLIC}
                       PRIV_INCLUDE_DIRS ${AOSL_ADD_INCLUDES_PRIVATE}
                       REQUIRES spi_flash newlib lwip mbedtls freertos esp_netif)
target_compile_options(${COMPONENT_LIB} PRIVATE ${AOSL_ADD_OPTIONS})
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${AOSL_ADD_DEFINITIONS})
