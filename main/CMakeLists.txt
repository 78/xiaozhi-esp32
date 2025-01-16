set(SOURCES "audio_codecs/audio_codec.cc"
            "audio_codecs/no_audio_codec.cc"
            "audio_codecs/box_audio_codec.cc"
            "audio_codecs/es8311_audio_codec.cc"
            "audio_codecs/cores3_audio_codec.cc"
            "led/single_led.cc"
            "led/circular_strip.cc"
            "display/display.cc"
            "display/no_display.cc"
            "display/lcd_display.cc"
            "display/ssd1306_display.cc"
            "protocols/protocol.cc"
            "protocols/mqtt_protocol.cc"
            "protocols/websocket_protocol.cc"
            "iot/thing.cc"
            "iot/thing_manager.cc"
            "system_info.cc"
            "application.cc"
            "ota.cc"
            "settings.cc"
            "background_task.cc"
            "main.cc"
            )

set(INCLUDE_DIRS "." "display" "audio_codecs" "protocols" "audio_processing")

# 添加 IOT 相关文件
file(GLOB IOT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/iot/things/*.cc)
list(APPEND SOURCES ${IOT_SOURCES})

# 字体
file(GLOB FONT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/fonts/*.c)
list(APPEND SOURCES ${FONT_SOURCES})
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/fonts)

# 添加板级公共文件
file(GLOB BOARD_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/*.cc)
list(APPEND SOURCES ${BOARD_COMMON_SOURCES})
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/common)

# 根据 BOARD_TYPE 配置添加对应的板级文件
if(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI)
    set(BOARD_TYPE "bread-compact-wifi")
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_ML307)
    set(BOARD_TYPE "bread-compact-ml307")
elseif(CONFIG_BOARD_TYPE_ESP_BOX_3)
    set(BOARD_TYPE "esp-box-3")
elseif(CONFIG_BOARD_TYPE_KEVIN_BOX_1)
    set(BOARD_TYPE "kevin-box-1")
elseif(CONFIG_BOARD_TYPE_KEVIN_BOX_2)
    set(BOARD_TYPE "kevin-box-2")
elseif(CONFIG_BOARD_TYPE_KEVIN_C3)
    set(BOARD_TYPE "kevin-c3")
elseif(CONFIG_BOARD_TYPE_LICHUANG_DEV)
    set(BOARD_TYPE "lichuang-dev")
elseif(CONFIG_BOARD_TYPE_LICHUANG_C3_DEV)
    set(BOARD_TYPE "lichuang-c3-dev")
elseif(CONFIG_BOARD_TYPE_MAGICLICK_2P4)
    set(BOARD_TYPE "magiclick-2p4")
elseif(CONFIG_BOARD_TYPE_M5STACK_CORE_S3)
    set(BOARD_TYPE "m5stack-core-s3")
elseif(CONFIG_BOARD_TYPE_ATOMS3R_ECHO_BASE)
    set(BOARD_TYPE "atoms3r-echo-base")
elseif(CONFIG_BOARD_TYPE_XMINI_C3)
    set(BOARD_TYPE "xmini-c3")
elseif(CONFIG_BOARD_TYPE_ESP32S3_KORVO2_V3)
    set(BOARD_TYPE "esp32s3-korvo2-v3")
elseif(CONFIG_BOARD_TYPE_ESP_SPARKBOT)
    set(BOARD_TYPE "esp-sparkbot")    
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_AMOLED_1_8)
    set(BOARD_TYPE "esp32-s3-touch-amoled-1.8")
endif()
file(GLOB BOARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc)
list(APPEND SOURCES ${BOARD_SOURCES})

if(CONFIG_IDF_TARGET_ESP32S3)
    list(APPEND SOURCES "audio_processing/audio_processor.cc" "audio_processing/wake_word_detect.cc")
endif()

idf_component_register(SRCS ${SOURCES}
                    EMBED_FILES "assets/err_reg.p3" "assets/err_pin.p3" "assets/wificonfig.p3" "assets/upgrade.p3"
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    WHOLE_ARCHIVE
                    )

# 使用 target_compile_definitions 来定义 BOARD_TYPE
target_compile_definitions(${COMPONENT_LIB}
                    PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\"
                    )
