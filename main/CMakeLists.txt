set(SOURCES "audio_codec.cc"
            "audio_codecs/no_audio_codec.cc"
            "audio_codecs/box_audio_codec.cc"
            "display.cc"
            "display/no_display.cc"
            "display/st7789_display.cc"
            "display/ssd1306_display.cc"
            "display/font_dingding.c"
            "board.cc"
            "boards/wifi_board.cc"
            "boards/ml307_board.cc"
            "system_info.cc"
            "system_reset.cc"
            "application.cc"
            "button.cc"
            "led.cc"
            "ota.cc"
            "main.cc"

            )
set(INCLUDE_DIRS ".")

# 根据 BOARD_TYPE 配置添加对应的板级文件
if(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI)
    set(BOARD_TYPE "bread-compact-wifi")
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_ML307)
    set(BOARD_TYPE "bread-compact-ml307")
elseif(CONFIG_BOARD_TYPE_ESP_BOX_3)
    set(BOARD_TYPE "esp-box-3")
elseif(CONFIG_BOARD_TYPE_KEVIN_BOX_0)
    set(BOARD_TYPE "kevin-box-0")
elseif(CONFIG_BOARD_TYPE_KEVIN_BOX_1)
    set(BOARD_TYPE "kevin-box-1")
elseif(CONFIG_BOARD_TYPE_LICHUANG_DEV)
    set(BOARD_TYPE "lichuang-dev")
elseif(CONFIG_BOARD_TYPE_KEVIN_SP_V3_DEV)
    set(BOARD_TYPE "kevin-sp-v3-dev")
elseif(CONFIG_BOARD_TYPE_ESP32S3_KORVO2_V3)
    set(BOARD_TYPE "esp32s3_korvo2_v3")
endif()
file(GLOB BOARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc)
list(APPEND SOURCES ${BOARD_SOURCES})
list(APPEND INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE})

if(CONFIG_USE_AFE_SR)
    list(APPEND SOURCES "audio_processor.cc" "wake_word_detect.cc")
endif()

idf_component_register(SRCS ${SOURCES}
                    EMBED_FILES "assets/err_reg.p3" "assets/err_pin.p3" "assets/err_wificonfig.p3"
                    INCLUDE_DIRS ${INCLUDE_DIRS} ${INCLUDES} 
                    )

# 使用 target_compile_definitions 来定义 BOARD_TYPE
target_compile_definitions(${COMPONENT_LIB}
                    PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\"
                    )
