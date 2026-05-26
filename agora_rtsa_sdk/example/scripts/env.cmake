set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

macro(SUBDIRLIST result curdir)
  file(
    GLOB children
    RELATIVE ${curdir}
    ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
      list(APPEND dirlist ${child})
    endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()

find_path(third_party_path NAMES third-party PATHS ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/../../ NO_DEFAULT_PATH REQUIRED)
find_path(utility_path NAMES utility PATHS ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/../ NO_DEFAULT_PATH REQUIRED)
set(THIRD_PARTY ${third_party_path}/third-party)
set(UTILITY ${utility_path}/utility)

if(ANDROID)
  # Android bionic libc already includes pthread, dl, rt and math support
  set(LIBS "")
elseif(APPLE)
  set(LIBS "pthread" "m")
else()
  set(LIBS "pthread" "dl" "rt" "m")
endif()

if(APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,. -Wl,-rpath,../../../agora_sdk/lib/${MACHINE}")
else()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath=.:../../../agora_sdk/lib/${MACHINE}")
endif()

# Build Types
set(CMAKE_BUILD_TYPE
    ${CMAKE_BUILD_TYPE}
    CACHE STRING "Choose the type of build, options are: None Debug Release asan"
      FORCE)

# Debug
set(CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS_DEBUG} -g -O1 -DENABLE_DEBUG=1"
    CACHE STRING "Flags used by the C compiler during AddressSanitizer builds."
          FORCE)
# Release
set(CMAKE_C_FLAGS_RELEASE
    "${CMAKE_C_FLAGS_RELEASE} -g -O1 -DENABLE_DEBUG=1"
    CACHE STRING "Flags used by the C compiler during AddressSanitizer builds."
          FORCE)

# AddressSanitize
set(CMAKE_C_FLAGS_ASAN
    "${CMAKE_C_FLAGS_ASAN} -fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C compiler during AddressSanitizer builds."
          FORCE)
set(CMAKE_CXX_FLAGS_ASAN
    "${CMAKE_CXX_FLAGS_ASAN} -fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
    CACHE STRING
          "Flags used by the C++ compiler during AddressSanitizer builds."
          FORCE)

message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
