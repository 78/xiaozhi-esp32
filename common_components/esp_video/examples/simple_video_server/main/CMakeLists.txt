set(srcs "simple_video_server_example.c")

idf_component_register(SRCS "${srcs}"
                       PRIV_INCLUDE_DIRS .)
