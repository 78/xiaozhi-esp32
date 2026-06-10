# CMakeLists.txt — Placa personalizada ESP32-C6-DevKitC-1
#
# Este archivo le indica a ESP-IDF qué fuentes compilar para esta placa.
# Se incluye automáticamente cuando CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_ESP32C6=y

idf_component_register(
    SRCS
        "esp32_c6_devkitc1.cc"     # Implementa create_board() ← CRÍTICO
    INCLUDE_DIRS
        "."                         # Expone board_config.h al resto del proyecto
    PRIV_REQUIRES
        driver                      # i2c_master, gpio, i2s
        esp_driver_i2c              # API nueva I2C (ESP-IDF v5.x)
        button                      # Componente espressif__button
        display                     # Subsistema de display de XiaoZhi
        board                       # Clase base Board
        application                 # Application::GetInstance()
        led                         # SingleLed / Led
        esp_log                     # ESP_LOGI / ESP_LOGE
)
