set(srcs "app_main.c")

idf_component_register(SRCS "${srcs}"
                       PRIV_INCLUDE_DIRS .)
