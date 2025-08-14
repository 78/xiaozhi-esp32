set(SOURCES "audio/audio_codec.cc"
            "audio/audio_service.cc"
            "audio/codecs/no_audio_codec.cc"
            "audio/codecs/box_audio_codec.cc"
            "audio/codecs/es8311_audio_codec.cc"
            "audio/codecs/es8374_audio_codec.cc"
            "audio/codecs/es8388_audio_codec.cc"
            "audio/codecs/es8389_audio_codec.cc"
            "audio/codecs/dummy_audio_codec.cc"
            "audio/processors/audio_debugger.cc"
            "led/single_led.cc"
            "led/circular_strip.cc"
            "led/gpio_led.cc"
            "display/display.cc"
            "display/lcd_display.cc"
            "display/oled_display.cc"
            "protocols/protocol.cc"
            "protocols/mqtt_protocol.cc"
            "protocols/websocket_protocol.cc"
            "mcp_server.cc"
            "system_info.cc"
            "application.cc"
            "ota.cc"
            "settings.cc"
            "device_state_event.cc"
            "main.cc"
            )

set(INCLUDE_DIRS "." "display" "audio" "protocols")

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
elseif(CONFIG_BOARD_TYPE_DF_S3_AI_CAM)
    set(BOARD_TYPE "df-s3-ai-cam")
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
elseif(CONFIG_BOARD_TYPE_M5STACK_CORE_TAB5)
    set(BOARD_TYPE "m5stack-tab5")
elseif(CONFIG_BOARD_TYPE_ATOMS3_ECHO_BASE)
    set(BOARD_TYPE "atoms3-echo-base")
elseif(CONFIG_BOARD_TYPE_ATOMS3R_ECHO_BASE)
    set(BOARD_TYPE "atoms3r-echo-base")
elseif(CONFIG_BOARD_TYPE_ATOMS3R_CAM_M12_ECHO_BASE)
    set(BOARD_TYPE "atoms3r-cam-m12-echo-base")
elseif(CONFIG_BOARD_TYPE_ATOMMATRIX_ECHO_BASE)
    set(BOARD_TYPE "atommatrix-echo-base")
elseif(CONFIG_BOARD_TYPE_XMINI_C3_V3)
    set(BOARD_TYPE "xmini-c3-v3")
elseif(CONFIG_BOARD_TYPE_XMINI_C3_4G)
    set(BOARD_TYPE "xmini-c3-4g")
elseif(CONFIG_BOARD_TYPE_XMINI_C3)
    set(BOARD_TYPE "xmini-c3")
elseif(CONFIG_BOARD_TYPE_ESP32S3_KORVO2_V3)
    set(BOARD_TYPE "esp32s3-korvo2-v3")
elseif(CONFIG_BOARD_TYPE_ESP_SPARKBOT)
    set(BOARD_TYPE "esp-sparkbot")
elseif(CONFIG_BOARD_TYPE_ESP_SPOT_S3)
    set(BOARD_TYPE "esp-spot-s3")
elseif(CONFIG_BOARD_TYPE_ESP_HI)
    set(BOARD_TYPE "esp-hi")
elseif(CONFIG_BOARD_TYPE_ECHOEAR)
    set(BOARD_TYPE "echoear")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_AMOLED_1_8)
    set(BOARD_TYPE "esp32-s3-touch-amoled-1.8")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_AMOLED_2_06)
    set(BOARD_TYPE "waveshare-s3-touch-amoled-2.06")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_AMOLED_1_75)
    set(BOARD_TYPE "waveshare-s3-touch-amoled-1.75")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_LCD_1_85C)
    set(BOARD_TYPE "esp32-s3-touch-lcd-1.85c")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_LCD_1_85)
    set(BOARD_TYPE "esp32-s3-touch-lcd-1.85")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_LCD_1_46)
    set(BOARD_TYPE "esp32-s3-touch-lcd-1.46")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_LCD_3_5)
    set(BOARD_TYPE "esp32-s3-touch-lcd-3.5")
elseif(CONFIG_BOARD_TYPE_ESP32S3_Touch_LCD_3_5B)
    set(BOARD_TYPE "waveshare-s3-touch-lcd-3.5b")
elseif(CONFIG_BOARD_TYPE_ESP32C6_LCD_1_69)
    set(BOARD_TYPE "waveshare-c6-lcd-1.69")
elseif(CONFIG_BOARD_TYPE_ESP32C6_Touch_AMOLED_1_43)
    set(BOARD_TYPE "waveshare-c6-touch-amoled-1.43")
elseif(CONFIG_BOARD_TYPE_ESP32P4_NANO)
    set(BOARD_TYPE "waveshare-p4-nano")
elseif(CONFIG_BOARD_TYPE_ESP32P4_WIFI6_Touch_LCD_4B)
    set(BOARD_TYPE "waveshare-p4-wifi6-touch-lcd-4b")
elseif(CONFIG_BOARD_TYPE_ESP32P4_WIFI6_Touch_LCD_XC)
    set(BOARD_TYPE "waveshare-p4-wifi6-touch-lcd-xc")
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD)
    set(BOARD_TYPE "bread-compact-wifi-lcd")
elseif(CONFIG_BOARD_TYPE_TUDOUZI)
    set(BOARD_TYPE "tudouzi")
elseif(CONFIG_BOARD_TYPE_LILYGO_T_CIRCLE_S3)
    set(BOARD_TYPE "lilygo-t-circle-s3")
elseif(CONFIG_BOARD_TYPE_LILYGO_T_CAMERAPLUS_S3_V1_0_V1_1)
    set(BOARD_TYPE "lilygo-t-cameraplus-s3")
elseif(CONFIG_BOARD_TYPE_LILYGO_T_CAMERAPLUS_S3_V1_2)
    set(BOARD_TYPE "lilygo-t-cameraplus-s3")
elseif(CONFIG_BOARD_TYPE_LILYGO_T_DISPLAY_S3_PRO_MVSRLORA)
    set(BOARD_TYPE "lilygo-t-display-s3-pro-mvsrlora")
elseif(CONFIG_BOARD_TYPE_LILYGO_T_DISPLAY_S3_PRO_MVSRLORA_NO_BATTERY)
    set(BOARD_TYPE "lilygo-t-display-s3-pro-mvsrlora")
elseif(CONFIG_BOARD_TYPE_MOVECALL_MOJI_ESP32S3)
    set(BOARD_TYPE "movecall-moji-esp32s3")
    elseif(CONFIG_BOARD_TYPE_MOVECALL_CUICAN_ESP32S3)
    set(BOARD_TYPE "movecall-cuican-esp32s3")
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3)
    set(BOARD_TYPE "atk-dnesp32s3")
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3_BOX)
    set(BOARD_TYPE "atk-dnesp32s3-box")
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3_BOX0)
    set(BOARD_TYPE "atk-dnesp32s3-box0")
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3_BOX2_WIFI)
    set(BOARD_TYPE "atk-dnesp32s3-box2-wifi")
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3_BOX2_4G)
    set(BOARD_TYPE "atk-dnesp32s3-box2-4g")   
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3M_WIFI)
    set(BOARD_TYPE "atk-dnesp32s3m-wifi")
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3M_4G)
    set(BOARD_TYPE "atk-dnesp32s3m-4g")
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
elseif(CONFIG_BOARD_TYPE_DOIT_S3_AIBOX)
    set(BOARD_TYPE "doit-s3-aibox")
elseif(CONFIG_BOARD_TYPE_MIXGO_NOVA)
    set(BOARD_TYPE "mixgo-nova")
elseif(CONFIG_BOARD_TYPE_GENJUTECH_S3_1_54TFT)
    set(BOARD_TYPE "genjutech-s3-1.54tft")
elseif(CONFIG_BOARD_TYPE_ESP32_CGC)
    set(BOARD_TYPE "esp32-cgc")
elseif(CONFIG_BOARD_TYPE_ESP32_CGC_144)
    set(BOARD_TYPE "esp32-cgc-144")  
elseif(CONFIG_BOARD_TYPE_ESP_S3_LCD_EV_Board)
    set(BOARD_TYPE "esp-s3-lcd-ev-board")
elseif(CONFIG_BOARD_TYPE_ESP_S3_LCD_EV_Board_2)
    set(BOARD_TYPE "esp-s3-lcd-ev-board-2")
elseif(CONFIG_BOARD_TYPE_ZHENGCHEN_1_54TFT_WIFI)
    set(BOARD_TYPE "zhengchen-1.54tft-wifi")
elseif(CONFIG_BOARD_TYPE_MINSI_K08_DUAL)
    set(BOARD_TYPE "minsi-k08-dual")
elseif(CONFIG_BOARD_TYPE_ZHENGCHEN_1_54TFT_ML307)
    set(BOARD_TYPE "zhengchen-1.54tft-ml307")
elseif(CONFIG_BOARD_TYPE_ESP32_S3_1_54_MUMA)
    set(BOARD_TYPE "sp-esp32-s3-1.54-muma")
elseif(CONFIG_BOARD_TYPE_ESP32_S3_1_28_BOX)
    set(BOARD_TYPE "sp-esp32-s3-1.28-box")
elseif(CONFIG_BOARD_TYPE_OTTO_ROBOT)
    set(BOARD_TYPE "otto-robot")
elseif(CONFIG_BOARD_TYPE_ELECTRON_BOT)
    set(BOARD_TYPE "electron-bot")
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_CAM)
    set(BOARD_TYPE "bread-compact-wifi-s3cam")
elseif(CONFIG_BOARD_TYPE_JIUCHUAN)
    set(BOARD_TYPE "jiuchuan-s3")
elseif(CONFIG_BOARD_TYPE_LABPLUS_MPYTHON_V3)
    set(BOARD_TYPE "labplus-mpython-v3") 
elseif(CONFIG_BOARD_TYPE_LABPLUS_LEDONG_V2)
    set(BOARD_TYPE "labplus-ledong-v2") 
elseif(CONFIG_BOARD_TYPE_SURFER_C3_1_14TFT)
    set(BOARD_TYPE "surfer-c3-1.14tft")
endif()
file(GLOB BOARD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.c
)
list(APPEND SOURCES ${BOARD_SOURCES})

if(CONFIG_USE_AUDIO_PROCESSOR)
    list(APPEND SOURCES "audio/processors/afe_audio_processor.cc")
else()
    list(APPEND SOURCES "audio/processors/no_audio_processor.cc")
endif()
if(CONFIG_USE_AFE_WAKE_WORD)
    list(APPEND SOURCES "audio/wake_words/afe_wake_word.cc")
elseif(CONFIG_USE_ESP_WAKE_WORD)
    list(APPEND SOURCES "audio/wake_words/esp_wake_word.cc")
elseif(CONFIG_USE_CUSTOM_WAKE_WORD)
    list(APPEND SOURCES "audio/wake_words/custom_wake_word.cc")
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
elseif(CONFIG_LANGUAGE_KO_KR)
    set(LANG_DIR "ko-KR")
elseif(CONFIG_LANGUAGE_VI_VN)
    set(LANG_DIR "vi-VN")
elseif(CONFIG_LANGUAGE_TH_TH)
    set(LANG_DIR "th-TH")
elseif(CONFIG_LANGUAGE_DE_DE)
    set(LANG_DIR "de-DE")
elseif(CONFIG_LANGUAGE_FR_FR)
    set(LANG_DIR "fr-FR")
elseif(CONFIG_LANGUAGE_ES_ES)
    set(LANG_DIR "es-ES")
elseif(CONFIG_LANGUAGE_IT_IT)
    set(LANG_DIR "it-IT")
elseif(CONFIG_LANGUAGE_RU_RU)
    set(LANG_DIR "ru-RU")
elseif(CONFIG_LANGUAGE_AR_SA)
    set(LANG_DIR "ar-SA")
elseif(CONFIG_LANGUAGE_HI_IN)
    set(LANG_DIR "hi-IN")
elseif(CONFIG_LANGUAGE_PT_PT)
    set(LANG_DIR "pt-PT")
elseif(CONFIG_LANGUAGE_PL_PL)
    set(LANG_DIR "pl-PL")
elseif(CONFIG_LANGUAGE_CS_CZ)
    set(LANG_DIR "cs-CZ")
elseif(CONFIG_LANGUAGE_FI_FI)
    set(LANG_DIR "fi-FI")
elseif(CONFIG_LANGUAGE_TR_TR)
    set(LANG_DIR "tr-TR")
elseif(CONFIG_LANGUAGE_ID_ID)
    set(LANG_DIR "id-ID")
elseif(CONFIG_LANGUAGE_UK_UA)
    set(LANG_DIR "uk-UA")
elseif(CONFIG_LANGUAGE_RO_RO)
    set(LANG_DIR "ro-RO")
endif()

# 定义生成路径
set(LANG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/${LANG_DIR}/language.json")
set(LANG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/assets/lang_config.h")
file(GLOB LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/${LANG_DIR}/*.ogg)
file(GLOB COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/common/*.ogg)

# 如果目标芯片是 ESP32，则排除特定文件
if(CONFIG_IDF_TARGET_ESP32)
    list(REMOVE_ITEM SOURCES "audio/codecs/box_audio_codec.cc"
                             "audio/codecs/es8388_audio_codec.cc"
                             "audio/codecs/es8389_audio_codec.cc"
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
            --language "${LANG_DIR}"
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

if(CONFIG_BOARD_TYPE_ESP_HI)
set(URL "https://github.com/espressif2022/image_player/raw/main/test_apps/test_8bit")
set(SPIFFS_DIR "${CMAKE_BINARY_DIR}/emoji")
file(MAKE_DIRECTORY ${SPIFFS_DIR})

# List all files to download
set(FILES_TO_DOWNLOAD "")
list(APPEND FILES_TO_DOWNLOAD "Anger_enter.aaf" "Anger_loop.aaf" "Anger_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "happy_enter.aaf" "happy_loop.aaf" "happ_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "sad_enter.aaf" "sad_loop.aaf" "sad_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "scorn_enter.aaf" "scorn_loop.aaf" "scorn_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "left_enter.aaf" "left_loop.aaf" "left_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "right_enter.aaf" "right_loop.aaf" "right_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "asking.aaf" "blink_once.aaf" "blink_quick.aaf")
list(APPEND FILES_TO_DOWNLOAD "connecting.aaf" "panic_enter.aaf" "panic_loop.aaf")
list(APPEND FILES_TO_DOWNLOAD "panic_return.aaf" "wake.aaf")

foreach(FILENAME IN LISTS FILES_TO_DOWNLOAD)
    set(REMOTE_FILE "${URL}/${FILENAME}")
    set(LOCAL_FILE "${SPIFFS_DIR}/${FILENAME}")
    
    # 检查本地文件是否存在
    if(EXISTS ${LOCAL_FILE})
        message(STATUS "File ${FILENAME} already exists, skipping download")
    else()
        message(STATUS "Downloading ${FILENAME}")
        file(DOWNLOAD ${REMOTE_FILE} ${LOCAL_FILE}
             STATUS DOWNLOAD_STATUS)
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download ${FILENAME} from ${URL}")
        endif()
    endif()
endforeach()

spiffs_create_partition_assets(
    assets_A
    ${SPIFFS_DIR}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf"
)
endif()

if(CONFIG_BOARD_TYPE_ECHOEAR)

idf_build_get_property(build_components BUILD_COMPONENTS)
foreach(COMPONENT ${build_components})
    if(COMPONENT MATCHES "esp_emote_gfx" OR COMPONENT MATCHES "espressif2022__esp_emote_gfx")
        set(EMOTE_GFX_COMPONENT ${COMPONENT})
        idf_component_get_property(EMOTE_GFX_COMPONENT_PATH ${EMOTE_GFX_COMPONENT} COMPONENT_DIR)
        set(SPIFFS_DIR "${EMOTE_GFX_COMPONENT_PATH}/emoji_normal")
        break()
    endif()
endforeach()

spiffs_create_partition_assets(
    assets_A
    ${SPIFFS_DIR}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf, ttf, bin"
    IMPORT_INC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}
)
endif()