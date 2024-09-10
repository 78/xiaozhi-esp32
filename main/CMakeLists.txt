set(SOURCES "AudioDevice.cc"
            "SystemReset.cc"
            "Application.cc"
            "main.cc"
            )

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS "."
                    )
