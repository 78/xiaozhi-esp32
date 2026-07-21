set(AOSL_DIR ${COMPONENT_PATH})
set(CONFIG_PLATFORM "esp32-s3")
set(AOSL_DECLARE_PROJECT OFF CACHE BOOL "Declare as Standalone Project" FORCE)

# Initialize build variables
set(AOSL_ADD_OPTIONS)
set(AOSL_ADD_DEFINITIONS)
set(AOSL_ADD_INCLUDES_PUBLIC)
set(AOSL_ADD_INCLUDES_PRIVATE)
set(AOSL_ADD_SOURCES)

# Include the aosl build script which populates the above variables
include(${AOSL_DIR}/cmake/build.cmake)

idf_component_register(SRCS ${AOSL_ADD_SOURCES}
                       INCLUDE_DIRS ${AOSL_ADD_INCLUDES_PUBLIC}
                       PRIV_INCLUDE_DIRS ${AOSL_ADD_INCLUDES_PRIVATE}
                       REQUIRES spi_flash lwip mbedtls freertos esp_netif)
target_compile_options(${COMPONENT_LIB} PRIVATE ${AOSL_ADD_OPTIONS})
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${AOSL_ADD_DEFINITIONS})
