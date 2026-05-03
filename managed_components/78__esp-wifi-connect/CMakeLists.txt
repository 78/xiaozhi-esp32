message("78/esp_wifi_connect: IDF_VER is ${IDF_VER}")

if(${IDF_VER} STRGREATER_EQUAL "v6.0")
    # v6
    message("78/esp_wifi_connect: idf_component_register for v6 used")

    idf_component_register(
        SRCS
            "wifi_manager.cc"
            "wifi_configuration_ap.cc"
            "wifi_station.cc"
            "ssid_manager.cc"
            "dns_server.cc"
        INCLUDE_DIRS
            "include"
        EMBED_TXTFILES
            "assets/wifi_configuration.html"
            "assets/wifi_configuration_done.html"
        REQUIRES "esp_timer" "esp_http_server" "esp_wifi" "nvs_flash" "app_update"
    )
else()
    # v5
    message("78/esp_wifi_connect: idf_component_register for v5 used")

    idf_component_register(
        SRCS
            "wifi_manager.cc"
            "wifi_configuration_ap.cc"
            "wifi_station.cc"
            "ssid_manager.cc"
            "dns_server.cc"
        INCLUDE_DIRS
            "include"
        EMBED_TXTFILES
            "assets/wifi_configuration.html"
            "assets/wifi_configuration_done.html"
        REQUIRES "esp_timer" "esp_http_server" "esp_wifi" "nvs_flash" "json" "app_update"
    )
endif()

