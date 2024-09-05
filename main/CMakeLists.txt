set(SOURCES "AudioDevice.cc"
            "SystemInfo.cc"
            "WebSocketClient.cc"
            "OpusEncoder.cc"
            "BuiltinLed.cc"
            "Application.cc"
            "WifiConfigurationAp.cc"
            "main.cc"
            "WifiStation.cc"
            "FirmwareUpgrade.cc"
            )

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "assets/wifi_configuration_ap.html"
                    )
