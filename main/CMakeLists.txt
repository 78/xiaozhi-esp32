set(SOURCES "AudioDevice.cc"
            "SystemInfo.cc"
            "SystemReset.cc"
            "WebSocketClient.cc"
            "OpusEncoder.cc"
           
            "Application.cc"
            "main.cc"
            "FirmwareUpgrade.cc"
            )

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS "."
                    )
