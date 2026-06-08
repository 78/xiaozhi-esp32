set(AOSL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/aosl)
set(CONFIG_PLATFORM "rtl8713")
set(AOSL_DECLARE_PROJECT OFF CACHE BOOL "Declare as Standalone Project" FORCE)

include(${AOSL_DIR}/CMakeLists.txt)

message(STATUS "------------aosl/CMakeLists.txt-----------------")
message(STATUS "AOSL_ADD_OPTIONS=${AOSL_ADD_OPTIONS}")
message(STATUS "AOSL_ADD_DEFINITIONS=${AOSL_ADD_DEFINITIONS}")
message(STATUS "AOSL_ADD_INCLUDES_PUBLIC=${AOSL_ADD_INCLUDES_PUBLIC}")
message(STATUS "AOSL_ADD_INCLUDES_PRIVATE=${AOSL_ADD_INCLUDES_PRIVATE}")
message(STATUS "AOSL_ADD_SOURCES=${AOSL_ADD_SOURCES}")

##########################################################################################
## * This part defines public part of the component
## * Public part will be used as global build configures for all component

set(public_includes ${AOSL_ADD_INCLUDES_PUBLIC})                #public include directories, NOTE: relative path is OK
set(public_definitions)             #public definitions
set(public_libraries)     #public libraries(files), NOTE: linked with whole-archive options

#WARNING: Fixed section, DO NOT change!
ameba_global_include(${public_includes})
ameba_global_define(${public_definitions})
ameba_global_library(${public_libraries}) #default: whole-archived

##########################################################################################
## * This part defines private part of the component
## * Private part is used to build target of current component
## * NOTE: The build API guarantees the global build configures(mentioned above)
## *       applied to the target automatically. So if any configure was already added
## *       to public above, it's unnecessary to add again below.

#NOTE: User defined section, add your private build configures here
# You may use if-else condition to set these predefined variable
# They are only for ameba_add_internal_library
set(private_sources ${AOSL_ADD_SOURCES})                 #private source files, NOTE: relative path is OK
set(private_includes ${AOSL_ADD_INCLUDES_PRIVATE})                #private include directories, NOTE: relative path is OK
set(private_definitions ${AOSL_ADD_DEFINITIONS})             #private definitions
set(private_compile_options ${AOSL_ADD_OPTIONS})         #private compile_options

# Component private part, user config end
#------------------------------#

#WARNING: Select right API based on your component's build system

###NOTE: For open-source ameba component, always build from source
ameba_add_internal_library(aosl
    p_SOURCES
        ${private_sources}
    p_INCLUDES
        ${private_includes}
    p_DEFINITIONS
        ${private_definitions}
    p_COMPILE_OPTIONS
        ${private_compile_options}
)

##########################################################################################
