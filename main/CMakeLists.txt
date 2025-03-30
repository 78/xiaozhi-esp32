set(SOURCES "audio_codecs/audio_codec.cc"
            "audio_codecs/no_audio_codec.cc"
            "audio_codecs/box_audio_codec.cc"
            "audio_codecs/es8311_audio_codec.cc"
            "audio_codecs/es8388_audio_codec.cc"
            "led/single_led.cc"
            "led/circular_strip.cc"
            "led/gpio_led.cc"
            "display/display.cc"
            "display/lcd_display.cc"
            "display/oled_display.cc"
            "protocols/protocol.cc"
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

# 添加板级公共文件
file(GLOB BOARD_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/*.cc)
list(APPEND SOURCES ${BOARD_COMMON_SOURCES})
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/common)

# 根据 BOARD_TYPE 配置添加对应的板级文件
if(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI)
    set(BOARD_TYPE "bread-compact-wifi")
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_ML307)
    set(BOARD_TYPE "bread-compact-ml307")
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_ESP32)
    set(BOARD_TYPE "bread-compact-esp32")
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_ESP32_LCD)
    set(BOARD_TYPE "bread-compact-esp32-lcd")    
elseif(CONFIG_BOARD_TYPE_DF_K10)
    set(BOARD_TYPE "df-k10")
elseif(CONFIG_BOARD_TYPE_ESP_BOX_3)
    set(BOARD_TYPE "esp-box-3")
elseif(CONFIG_BOARD_TYPE_ESP_BOX)
    set(BOARD_TYPE "esp-box")
elseif(CONFIG_BOARD_TYPE_ESP_BOX_LITE)
    set(BOARD_TYPE "esp-box-lite")
elseif(CONFIG_BOARD_TYPE_KEVIN_BOX_1)
    set(BOARD_TYPE "kevin-box-1")
elseif(CONFIG_BOARD_TYPE_KEVIN_BOX_2)
    set(BOARD_TYPE "kevin-box-2")
elseif(CONFIG_BOARD_TYPE_KEVIN_C3)
    set(BOARD_TYPE "kevin-c3")
elseif(CONFIG_BOARD_TYPE_KEVIN_SP_V3_DEV)
    set(BOARD_TYPE "kevin-sp-v3-dev")
elseif(CONFIG_BOARD_TYPE_KEVIN_SP_V4_DEV)
    set(BOARD_TYPE "kevin-sp-v4-dev")
elseif(CONFIG_BOARD_TYPE_KEVIN_YUYING_313LCD)
    set(BOARD_TYPE "kevin-yuying-313lcd")
elseif(CONFIG_BOARD_TYPE_LICHUANG_DEV)
    set(BOARD_TYPE "lichuang-dev")
elseif(CONFIG_BOARD_TYPE_LICHUANG_C3_DEV)
    set(BOARD_TYPE "lichuang-c3-dev")
elseif(CONFIG_BOARD_TYPE_MAGICLICK_2P4)
    set(BOARD_TYPE "magiclick-2p4")
elseif(CONFIG_BOARD_TYPE_MAGICLICK_2P5)
    set(BOARD_TYPE "magiclick-2p5")
elseif(CONFIG_BOARD_TYPE_MAGICLICK_C3)
    set(BOARD_TYPE "magiclick-c3")
elseif(CONFIG_BOARD_TYPE_MAGICLICK_C3_V2)
    set(BOARD_TYPE "magiclick-c3-v2")
elseif(CONFIG_BOARD_TYPE_M5STACK_CORE_S3)
    set(BOARD_TYPE "m5stack-core-s3")
elseif(CONFIG_BOARD_TYPE_ATOMS3_ECHO_BASE)
    set(BOARD_TYPE "atoms3-echo-base")
elseif(CONFIG_BOARD_TYPE_ATOMS3R_ECHO_BASE)
    set(BOARD_TYPE "atoms3r-echo-base")
elseif(CONFIG_BOARD_TYPE_ATOMS3R_CAM_M12_ECHO_BASE)
    set(BOARD_TYPE "atoms3r-cam-m12-echo-base")
elseif(CONFIG_BOARD_TYPE_ATOMMATRIX_ECHO_BASE)
    set(BOARD_TYPE "atommatrix-echo-base")
elseif(CONFIG_BOARD_TYPE_XMINI_C3)
    set(BOARD_TYPE "xmini-c3")
elseif(CONFIG_BOARD_TYPE_ESP32S3_KORVO2_V3)
    set(BOARD_TYPE "esp32s3-korvo2-v3")
elseif(CONFIG_BOARD_TYPE_ESP_SPARKBOT)
    set(BOARD_TYPE "esp-sparkbot")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_AMOLED_1_8)
    set(BOARD_TYPE "esp32-s3-touch-amoled-1.8")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_LCD_1_85C)
    set(BOARD_TYPE "esp32-s3-touch-lcd-1.85c")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_LCD_1_85)
    set(BOARD_TYPE "esp32-s3-touch-lcd-1.85")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_LCD_1_46)
    set(BOARD_TYPE "esp32-s3-touch-lcd-1.46")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_LCD_3_5)
    set(BOARD_TYPE "esp32-s3-touch-lcd-3.5")
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD)
    set(BOARD_TYPE "bread-compact-wifi-lcd")
elseif(CONFIG_BOARD_TYPE_TUDOUZI)
    set(BOARD_TYPE "tudouzi")
elseif(CONFIG_BOARD_TYPE_LILYGO_T_CIRCLE_S3)
    set(BOARD_TYPE "lilygo-t-circle-s3")
elseif(CONFIG_BOARD_TYPE_LILYGO_T_CAMERAPLUS_S3)
    set(BOARD_TYPE "lilygo-t-cameraplus-s3")
elseif(CONFIG_BOARD_TYPE_MOVECALL_MOJI_ESP32S3)
    set(BOARD_TYPE "movecall-moji-esp32s3")
    elseif(CONFIG_BOARD_TYPE_MOVECALL_CUICAN_ESP32S3)
    set(BOARD_TYPE "movecall-cuican-esp32s3")
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3)
    set(BOARD_TYPE "atk-dnesp32s3")
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3_BOX)
    set(BOARD_TYPE "atk-dnesp32s3-box")
elseif(CONFIG_BOARD_TYPE_DU_CHATX)
    set(BOARD_TYPE "du-chatx")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Taiji_Pi)
    set(BOARD_TYPE "taiji-pi-s3")
elseif(CONFIG_BOARD_TYPE_XINGZHI_Cube_0_85TFT_WIFI)
    set(BOARD_TYPE "xingzhi-cube-0.85tft-wifi")
elseif(CONFIG_BOARD_TYPE_XINGZHI_Cube_0_85TFT_ML307)
    set(BOARD_TYPE "xingzhi-cube-0.85tft-ml307")
elseif(CONFIG_BOARD_TYPE_XINGZHI_Cube_0_96OLED_WIFI)
    set(BOARD_TYPE "xingzhi-cube-0.96oled-wifi")
elseif(CONFIG_BOARD_TYPE_XINGZHI_Cube_0_96OLED_ML307)
    set(BOARD_TYPE "xingzhi-cube-0.96oled-ml307")
elseif(CONFIG_BOARD_TYPE_XINGZHI_Cube_1_54TFT_WIFI)
    set(BOARD_TYPE "xingzhi-cube-1.54tft-wifi")
elseif(CONFIG_BOARD_TYPE_XINGZHI_Cube_1_54TFT_ML307)
    set(BOARD_TYPE "xingzhi-cube-1.54tft-ml307")
elseif(CONFIG_BOARD_TYPE_SENSECAP_WATCHER)
    set(BOARD_TYPE "sensecap-watcher")
elseif(CONFIG_BOARD_TYPE_ESP32_CGC)
    set(BOARD_TYPE "esp32-cgc")  
endif()
file(GLOB BOARD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.c
)
list(APPEND SOURCES ${BOARD_SOURCES})

if(CONFIG_CONNECTION_TYPE_MQTT_UDP)
    list(APPEND SOURCES "protocols/mqtt_protocol.cc")
elseif(CONFIG_CONNECTION_TYPE_WEBSOCKET)
    list(APPEND SOURCES "protocols/websocket_protocol.cc")
endif()

if(CONFIG_USE_AUDIO_PROCESSOR)
    list(APPEND SOURCES "audio_processing/audio_processor.cc")
endif()
if(CONFIG_USE_WAKE_WORD_DETECT)
    list(APPEND SOURCES "audio_processing/wake_word_detect.cc")
endif()

# 根据Kconfig选择语言目录
if(CONFIG_LANGUAGE_ZH_CN)
    set(LANG_DIR "zh-CN")
elseif(CONFIG_LANGUAGE_ZH_TW)
    set(LANG_DIR "zh-TW")
elseif(CONFIG_LANGUAGE_EN_US)
    set(LANG_DIR "en-US")
elseif(CONFIG_LANGUAGE_JA_JP)
    set(LANG_DIR "ja-JP")
endif()

# 定义生成路径
set(LANG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/assets/${LANG_DIR}/language.json")
set(LANG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/assets/lang_config.h")
file(GLOB LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/${LANG_DIR}/*.p3)
file(GLOB COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/common/*.p3)

# 如果目标芯片是 ESP32，则排除特定文件
if(CONFIG_IDF_TARGET_ESP32)
    list(REMOVE_ITEM SOURCES "audio_codecs/box_audio_codec.cc"
                             "audio_codecs/es8388_audio_codec.cc"
                             "led/gpio_led.cc"
                             )
endif()

idf_component_register(SRCS ${SOURCES}
                    EMBED_FILES ${LANG_SOUNDS} ${COMMON_SOUNDS}
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    WHOLE_ARCHIVE
                    )

# 使用 target_compile_definitions 来定义 BOARD_TYPE, BOARD_NAME
# 如果 BOARD_NAME 为空，则使用 BOARD_TYPE
if(NOT BOARD_NAME)
    set(BOARD_NAME ${BOARD_TYPE})
endif()
target_compile_definitions(${COMPONENT_LIB}
                    PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\" BOARD_NAME=\"${BOARD_NAME}\"
                    )

# 添加生成规则
add_custom_command(
    OUTPUT ${LANG_HEADER}
    COMMAND python ${PROJECT_DIR}/scripts/gen_lang.py
            --input "${LANG_JSON}"
            --output "${LANG_HEADER}"
    DEPENDS
        ${LANG_JSON}
        ${PROJECT_DIR}/scripts/gen_lang.py
    COMMENT "Generating ${LANG_DIR} language config"
)

# 强制建立生成依赖
add_custom_target(lang_header ALL
    DEPENDS ${LANG_HEADER}
)
