# Set a default build type if none was specified
if(__opus_buildtype)
  return()
endif()
set(__opus_buildtype INCLUDED)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  if(CMAKE_C_FLAGS)
    message(STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS})
  else()
    set(default_build_type "Release")
    message(
      STATUS
        "Setting build type to '${default_build_type}' as none was specified and no CFLAGS was exported."
      )
    set(CMAKE_BUILD_TYPE "${default_build_type}"
        CACHE STRING "Choose the type of build."
        FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE
                 PROPERTY STRINGS
                          "Debug"
                          "Release"
                          "MinSizeRel"
                          "RelWithDebInfo")
  endif()
endif()
