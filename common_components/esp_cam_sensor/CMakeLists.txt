set(srcs "src/esp_cam_sensor.c" "src/esp_cam_sensor_xclk.c")

set(include_dirs "include")
set(requires "driver" "esp_sccb_intf")
set(priv_requires "esp_driver_gpio")

if(CONFIG_CAMERA_XCLK_USE_LEDC)
    list(APPEND requires "esp_driver_ledc")
endif()

if(CONFIG_CAMERA_OV2640)
    list(APPEND srcs "sensors/ov2640/ov2640.c")
    list(APPEND include_dirs "sensors/ov2640/include")
    list(APPEND priv_include_dirs "sensors/ov2640/private_include")
endif()

if(CONFIG_CAMERA_OV5645)
    list(APPEND srcs "sensors/ov5645/ov5645.c")
    list(APPEND include_dirs "sensors/ov5645/include")
    list(APPEND priv_include_dirs "sensors/ov5645/private_include")
endif()

if(CONFIG_CAMERA_OV02C10)
    list(APPEND srcs "sensors/ov02c10/ov02c10.c")
    list(APPEND include_dirs "sensors/ov02c10/include")
    list(APPEND priv_include_dirs "sensors/ov02c10/private_include")
endif()

if(CONFIG_CAMERA_OV5647)
    list(APPEND srcs "sensors/ov5647/ov5647.c")
    list(APPEND include_dirs "sensors/ov5647/include")
    list(APPEND priv_include_dirs "sensors/ov5647/private_include")
endif()

if(CONFIG_CAMERA_SC2336)
    list(APPEND srcs "sensors/sc2336/sc2336.c")
    list(APPEND include_dirs "sensors/sc2336/include")
    list(APPEND priv_include_dirs "sensors/sc2336/private_include")
endif()

if(CONFIG_CAMERA_SC202CS)
    list(APPEND srcs "sensors/sc202cs/sc202cs.c")
    list(APPEND include_dirs "sensors/sc202cs/include")
    list(APPEND priv_include_dirs "sensors/sc202cs/private_include")
endif()

if(CONFIG_CAMERA_GC0308)
    list(APPEND srcs "sensors/gc0308/gc0308.c")
    list(APPEND include_dirs "sensors/gc0308/include")
    list(APPEND priv_include_dirs "sensors/gc0308/private_include")
endif()

if(CONFIG_CAMERA_GC2145)
    list(APPEND srcs "sensors/gc2145/gc2145.c")
    list(APPEND include_dirs "sensors/gc2145/include")
    list(APPEND priv_include_dirs "sensors/gc2145/private_include")
endif()

if(CONFIG_CAMERA_SC101IOT)
    list(APPEND srcs "sensors/sc101iot/sc101iot.c")
    list(APPEND include_dirs "sensors/sc101iot/include")
    list(APPEND priv_include_dirs "sensors/sc101iot/private_include")
endif()

if(CONFIG_CAMERA_SC030IOT)
    list(APPEND srcs "sensors/sc030iot/sc030iot.c")
    list(APPEND include_dirs "sensors/sc030iot/include")
    list(APPEND priv_include_dirs "sensors/sc030iot/private_include")
endif()

if(CONFIG_CAMERA_OV2710)
    list(APPEND srcs "sensors/ov2710/ov2710.c")
    list(APPEND include_dirs "sensors/ov2710/include")
    list(APPEND priv_include_dirs "sensors/ov2710/private_include")
endif()

if(CONFIG_CAMERA_SC035HGS)
    list(APPEND srcs "sensors/sc035hgs/sc035hgs.c")
    list(APPEND include_dirs "sensors/sc035hgs/include")
    list(APPEND priv_include_dirs "sensors/sc035hgs/private_include")
endif()

if(CONFIG_CAMERA_BF3925)
    list(APPEND srcs "sensors/bf3925/bf3925.c")
    list(APPEND include_dirs "sensors/bf3925/include")
    list(APPEND priv_include_dirs "sensors/bf3925/private_include")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_INCLUDE_DIRS ${priv_include_dirs}
                       REQUIRES ${requires}
                       PRIV_REQUIRES ${priv_requires}
                       LDFRAGMENTS linker.lf)

if(CONFIG_CAMERA_OV2640_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov2640_detect")
endif()

if(CONFIG_CAMERA_OV5645_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov5645_detect")
endif()

if(CONFIG_CAMERA_OV5647_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov5647_detect")
endif()

if(CONFIG_CAMERA_OV02C10_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov02c10_detect")
endif()

if(CONFIG_CAMERA_SC2336_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc2336_detect")
endif()

if(CONFIG_CAMERA_SC202CS_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc202cs_detect")
endif()

if(CONFIG_CAMERA_GC0308_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u gc0308_detect")
endif()

if(CONFIG_CAMERA_GC2145_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u gc2145_detect")
endif()

if(CONFIG_CAMERA_SC101IOT_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc101iot_detect")
endif()

if(CONFIG_CAMERA_SC030IOT_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc030iot_detect")
endif()

if(CONFIG_CAMERA_OV2710_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov2710_detect")
endif()

if(CONFIG_CAMERA_SC035HGS_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc035hgs_detect")
endif()

if(CONFIG_CAMERA_BF3925_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u bf3925_detect")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
