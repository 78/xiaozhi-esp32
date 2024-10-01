set(SOURCES "AudioDevice.cc"
            "FirmwareUpgrade.cc"
            "SystemInfo.cc"
            "SystemReset.cc"
            "Application.cc"
            "Display.cc"
            "main.cc"
            )

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS "."
                    )
