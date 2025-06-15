idf_component_register(SRCS "esp_lcd_jd9365.c" INCLUDE_DIRS "include" PRIV_REQUIRES "driver" REQUIRES "esp_lcd")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
