############## Add hal files ###############
list(APPEND AOSL_ADD_INCLUDES_PUBLIC "${AOSL_DIR}/platform/src/${CONFIG_PLATFORM}/config")
file(GLOB   AOSL_PLATFORM_SRCS       "${AOSL_DIR}/platform/src/${CONFIG_PLATFORM}/*.c")
list(APPEND AOSL_ADD_SOURCES          ${AOSL_PLATFORM_SRCS})

############## Add definitions ###############
# Define AOSL config options with default values
# Format: CONFIG_NAME=default_value
# Can be overridden via cmake -DCONFIG_AOSL_IPV6=n etc.
set(AOSL_CONFIG_LIST
    CONFIG_AOSL_IPV6=y
    CONFIG_AOSL_MEM_STAT=n
    CONFIG_AOSL_MEM_DUMP=n
)

# Parse and apply config list
foreach(config_entry ${AOSL_CONFIG_LIST})
    string(REPLACE "=" ";" parts ${config_entry})
    list(GET parts 0 config_name)
    list(GET parts 1 default_value)
    
    # Use command line value if defined, otherwise use default
    if(NOT DEFINED ${config_name})
        set(${config_name} ${default_value})
    endif()
    
    # Add to definitions if not disabled
    set(val ${${config_name}})
    message(STATUS "AOSL Config: ${config_name} = ${val}")
    if(NOT val STREQUAL "n")
        list(APPEND AOSL_ADD_DEFINITIONS "-D${config_name}=${val}")
    endif()
endforeach()

################# Add include directory #######
list(APPEND AOSL_ADD_INCLUDES_PUBLIC
    "${AOSL_DIR}/include"
    "${AOSL_DIR}/platform/include")

############## Add source files ###############
list(APPEND AOSL_ADD_SOURCES
    "${AOSL_DIR}/kernel/fileobj.c"
    "${AOSL_DIR}/kernel/iofd.c"
    "${AOSL_DIR}/kernel/timer.c"
    "${AOSL_DIR}/kernel/osmp.c"
    "${AOSL_DIR}/kernel/mpq.c"
    "${AOSL_DIR}/kernel/mpqp.c"
    "${AOSL_DIR}/kernel/refobj.c"
    "${AOSL_DIR}/kernel/file.c"
    "${AOSL_DIR}/kernel/time.c"
    "${AOSL_DIR}/kernel/log.c"
    "${AOSL_DIR}/kernel/panic.c"
    "${AOSL_DIR}/kernel/version.c"
    "${AOSL_DIR}/kernel/thread.c"
    "${AOSL_DIR}/kernel/select_mp.c"
    "${AOSL_DIR}/kernel/poll_mp.c"
    "${AOSL_DIR}/kernel/et_mp.c"
    "${AOSL_DIR}/kernel/rt_monitor.c"

    "${AOSL_DIR}/lib/thread_api.c"
    "${AOSL_DIR}/lib/atomic.c"
    "${AOSL_DIR}/lib/bitmap.c"
    "${AOSL_DIR}/lib/rbtree.c"
    "${AOSL_DIR}/lib/marshalling.c"
    "${AOSL_DIR}/lib/marshalling-base-obj.c"
    "${AOSL_DIR}/lib/psbuff.c"
    "${AOSL_DIR}/lib/tls.c"
    "${AOSL_DIR}/lib/utils.c"
    "${AOSL_DIR}/lib/cond.c"
    "${AOSL_DIR}/lib/rwlockimpl.c"
    "${AOSL_DIR}/lib/rwlock.c"
    "${AOSL_DIR}/lib/byteswap.c"
    "${AOSL_DIR}/lib/errno.c"

    "${AOSL_DIR}/mm/mm.c"

    "${AOSL_DIR}/net/netifs.c"
    "${AOSL_DIR}/net/route.c"
    "${AOSL_DIR}/net/sk_utils.c"
    "${AOSL_DIR}/net/inet.c"
    "${AOSL_DIR}/net/dns.c"

    "${AOSL_DIR}/test/aosl_test.c"
)

if(CONFIG_AOSL_IPV6)
    list(APPEND AOSL_ADD_SOURCES "${AOSL_DIR}/net/ipv6.c")
endif()

############## Compile static lib ############
if(MSVC)
    list(APPEND AOSL_ADD_OPTIONS /wd4100 /wd4189 /wd4505)
    list(APPEND AOSL_ADD_DEFINITIONS -D_CRT_SECURE_NO_WARNINGS)
else()
    list(APPEND AOSL_ADD_OPTIONS -Wno-unused-variable -Wno-unused-function)
endif()

if (AOSL_DECLARE_PROJECT)
    set(LIB_NAME "aosl")
    add_library(${LIB_NAME} STATIC ${AOSL_ADD_SOURCES})
    target_include_directories(${LIB_NAME} PRIVATE ${AOSL_ADD_INCLUDES_PRIVATE})
    target_include_directories(${LIB_NAME} PUBLIC ${AOSL_ADD_INCLUDES_PUBLIC})
    target_compile_options(${LIB_NAME} PRIVATE ${AOSL_ADD_OPTIONS})
    target_compile_definitions(${LIB_NAME} PRIVATE ${AOSL_ADD_DEFINITIONS})
    message(STATUS "Library ${LIB_NAME} created")
endif()

############## Compile test bin ############
if (AOSL_DECLARE_PROJECT AND AOSL_COMPILE_TEST)
    add_executable(aosl_test ${AOSL_DIR}/test/aosl_test_main.c)
    target_include_directories(aosl_test PRIVATE ${AOSL_ADD_INCLUDES_PUBLIC})
    if(WIN32)
        target_link_libraries(aosl_test PRIVATE aosl ws2_32 iphlpapi bcrypt)
    elseif(ANDROID)
        target_link_libraries(aosl_test PRIVATE aosl)
    elseif(APPLE)
        target_link_libraries(aosl_test PRIVATE aosl "pthread" "m")
    else()
        target_link_libraries(aosl_test PRIVATE aosl "pthread" "dl" "rt" "m")
    endif()
    message(STATUS "aosl_test created")
endif()

############## Copy include file ############
if (AOSL_DECLARE_PROJECT)
    get_filename_component(ABS_BINARY_DIR "${CMAKE_BINARY_DIR}" ABSOLUTE)
    get_filename_component(ABS_AOSL_DIR "${AOSL_DIR}" ABSOLUTE)

    if (ABS_BINARY_DIR STREQUAL ABS_AOSL_DIR)
        message(WARNING "In-source builds are not recommended. Please use a separate build directory.")
        message(STATUS "Skipping header file copy for in-source build")
    else()
        add_custom_target(copy_aosl_include_files ALL
            # Clean and create directories
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/include
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include

            # Copy header files
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${AOSL_DIR}/include/api
                ${CMAKE_BINARY_DIR}/include/api

            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${AOSL_DIR}/platform/include/hal
                ${CMAKE_BINARY_DIR}/include/hal

            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${AOSL_DIR}/platform/src/${CONFIG_PLATFORM}/config/hal
                ${CMAKE_BINARY_DIR}/include/hal

            COMMENT "Copying AOSL header files to build directory"
            VERBATIM
        )
    endif()
endif()
