idf_component_register(SRCS "main.c" ${SRCS}
                       INCLUDE_DIRS ${HDRS}
                       PRIV_REQUIRES aosl esp_wifi nvs_flash)