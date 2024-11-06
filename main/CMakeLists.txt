set(SOURCES "audio_device.cc"
            "system_info.cc"
            "system_reset.cc"
            "application.cc"
            "button.cc"
            "builtin_led.cc"
            "display.cc"
            "board.cc"
            "ota.cc"
            "main.cc"
            
            )
set(INCLUDE_DIRS ".")

# 根据 BOARD_TYPE 配置添加对应的板级文件
if(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI)
    # add all files from boards/bread-compact-wifi
    set(BOARD_TYPE "bread-compact-wifi")
    file(GLOB BOARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc)
    list(APPEND SOURCES ${BOARD_SOURCES} "wifi_board.cc")
    list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE})
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_ML307)
    # add all files from boards/bread-compact-ml307
    set(BOARD_TYPE "bread-compact-ml307")
    file(GLOB BOARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc)
    list(APPEND SOURCES ${BOARD_SOURCES} "ml307_board.cc")
    list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE})
elseif(CONFIG_BOARD_TYPE_ESP_BOX_3)
    # add all files from boards/esp-box-3
    set(BOARD_TYPE "esp-box-3")
    file(GLOB BOARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc)
    list(APPEND SOURCES ${BOARD_SOURCES} "wifi_board.cc")
    list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE})
    list(APPEND SOURCES "box_audio_device.cc")
elseif(CONFIG_BOARD_TYPE_KEVIN_BOX_0)
    # add all files from boards/kevin-box-0
    set(BOARD_TYPE "kevin-box-0")
    file(GLOB BOARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc)
    list(APPEND SOURCES ${BOARD_SOURCES} "ml307_board.cc")
    list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE})
    list(APPEND SOURCES "box_audio_device.cc")
elseif(CONFIG_BOARD_TYPE_KEVIN_BOX_1)
    # add all files from boards/kevin-box-1
    set(BOARD_TYPE "kevin-box-1")
    file(GLOB BOARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc)
    list(APPEND SOURCES ${BOARD_SOURCES} "ml307_board.cc")
    list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE})
    list(APPEND SOURCES "box_audio_device.cc")
elseif(CONFIG_BOARD_TYPE_LICHUANG_DEV)
    # add all files from boards/lichuang-dev
    set(BOARD_TYPE "lichuang-dev")
    file(GLOB BOARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc)
    list(APPEND SOURCES ${BOARD_SOURCES} "wifi_board.cc")
    list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE})
    list(APPEND SOURCES "box_audio_device.cc")
elseif(CONFIG_BOARD_TYPE_ESP32S3_KORVO2_V3)
    # add all files from boards/esp32s3_korvo2_v3
    set(BOARD_TYPE "esp32s3_korvo2_v3")
    file(GLOB BOARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc)
    list(APPEND SOURCES ${BOARD_SOURCES} "wifi_board.cc")
    list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE})
    list(APPEND SOURCES "box_audio_device.cc"
            "lv_gui.c"
            "font_dingdingjinbuti.c"
            "avi_player.c"
            "file_manager.c"
            "img_src_mic_logo.c"
            "my_esp_lvgl_port.c")
    
endif()

if(CONFIG_USE_AFE_SR)
    list(APPEND SOURCES "audio_processor.cc" "wake_word_detect.cc")
endif()

idf_component_register(SRCS ${SOURCES}
                    EMBED_FILES "assets/err_reg.p3" "assets/err_pin.p3" "assets/err_wificonfig.p3"
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    )

# 使用 target_compile_definitions 来定义 BOARD_TYPE
target_compile_definitions(${COMPONENT_LIB}
                    PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\"
                    )
