if(__opus_functions)
  return()
endif()
set(__opus_functions INCLUDED)

function(get_library_version OPUS_LIBRARY_VERSION OPUS_LIBRARY_VERSION_MAJOR)
  file(STRINGS configure.ac opus_lt_current_string
       LIMIT_COUNT 1
       REGEX "OPUS_LT_CURRENT=")
  string(REGEX MATCH
               "OPUS_LT_CURRENT=([0-9]*)"
               _
               ${opus_lt_current_string})
  set(OPUS_LT_CURRENT ${CMAKE_MATCH_1})

  file(STRINGS configure.ac opus_lt_revision_string
       LIMIT_COUNT 1
       REGEX "OPUS_LT_REVISION=")
  string(REGEX MATCH
               "OPUS_LT_REVISION=([0-9]*)"
               _
               ${opus_lt_revision_string})
  set(OPUS_LT_REVISION ${CMAKE_MATCH_1})

  file(STRINGS configure.ac opus_lt_age_string
       LIMIT_COUNT 1
       REGEX "OPUS_LT_AGE=")
  string(REGEX MATCH
               "OPUS_LT_AGE=([0-9]*)"
               _
               ${opus_lt_age_string})
  set(OPUS_LT_AGE ${CMAKE_MATCH_1})

  math(EXPR OPUS_LIBRARY_VERSION_MAJOR "${OPUS_LT_CURRENT} - ${OPUS_LT_AGE}")
  set(OPUS_LIBRARY_VERSION_MINOR ${OPUS_LT_AGE})
  set(OPUS_LIBRARY_VERSION_PATCH ${OPUS_LT_REVISION})
  set(
    OPUS_LIBRARY_VERSION
    "${OPUS_LIBRARY_VERSION_MAJOR}.${OPUS_LIBRARY_VERSION_MINOR}.${OPUS_LIBRARY_VERSION_PATCH}"
    PARENT_SCOPE)
  set(OPUS_LIBRARY_VERSION_MAJOR ${OPUS_LIBRARY_VERSION_MAJOR} PARENT_SCOPE)
endfunction()

function(check_flag NAME FLAG)
  include(CheckCCompilerFlag)
  check_c_compiler_flag(${FLAG} ${NAME}_SUPPORTED)
endfunction()

include(CheckIncludeFile)

# This function determines if the compiler has support for SSE, SSE2, SSE4.1, AVX,
# AVX2 and FMA. Should the target systems potentially lack SSE support, the
# OPUS_MAY_HAVE_SSE option is recommended for use. If, however, the target system is
# assured to support SSE, the OPUS_PRESUME_SSE option can be employed, thus
# eliminating the necessity for an SSE runtime check.
function(opus_detect_sse COMPILER_SUPPORT_SIMD)
  message(STATUS "Check SIMD support by compiler")
  check_include_file(xmmintrin.h HAVE_XMMINTRIN_H) # SSE1
  if(HAVE_XMMINTRIN_H)
    if(MSVC)
      # different arch options for 32 and 64 bit target for MSVC
      if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        check_flag(SSE1 /arch:SSE)
      else()
        set(SSE1_SUPPORTED
            1
            PARENT_SCOPE)
      endif()
    else()
      check_flag(SSE1 -msse)
    endif()
  else()
    set(SSE1_SUPPORTED
        0
        PARENT_SCOPE)
  endif()

  check_include_file(emmintrin.h HAVE_EMMINTRIN_H) # SSE2
  if(HAVE_EMMINTRIN_H)
    if(MSVC)
      if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        check_flag(SSE2 /arch:SSE2)
      else()
        set(SSE2_SUPPORTED
            1
            PARENT_SCOPE)
      endif()
    else()
      check_flag(SSE2 -msse2)
    endif()
  else()
    set(SSE2_SUPPORTED
        0
        PARENT_SCOPE)
  endif()

  check_include_file(smmintrin.h HAVE_SMMINTRIN_H) # SSE4.1
  if(HAVE_SMMINTRIN_H)
    if(MSVC)
      if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        check_flag(SSE4_1 /arch:SSE2) # SSE2 and above
      else()
        set(SSE4_1_SUPPORTED
            1
            PARENT_SCOPE)
      endif()
    else()
      check_flag(SSE4_1 -msse4.1)
    endif()
  else()
    set(SSE4_1_SUPPORTED
        0
        PARENT_SCOPE)
  endif()

  check_include_file(immintrin.h HAVE_IMMINTRIN_H) # AVX2
  if(HAVE_IMMINTRIN_H)
    if(MSVC)
      check_flag(AVX2 /arch:AVX2)
    else()
      check_flag(AVX2 -mavx2 -mfma -mavx)
    endif()
  else()
    set(AVX2_SUPPORTED
        0
        PARENT_SCOPE)
  endif()

  if(SSE1_SUPPORTED OR SSE2_SUPPORTED OR SSE4_1_SUPPORTED OR AVX2_SUPPORTED)
    set(COMPILER_SUPPORT_SIMD 1 PARENT_SCOPE)
  else()
    message(STATUS "No SIMD support in compiler")
  endif()
endfunction()

function(opus_detect_neon COMPILER_SUPPORT_NEON)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "(arm|aarch64)")
    message(STATUS "Check NEON support by compiler")
    check_include_file(arm_neon.h HAVE_ARM_NEON_H)
    if(HAVE_ARM_NEON_H)
      set(COMPILER_SUPPORT_NEON ${HAVE_ARM_NEON_H} PARENT_SCOPE)
    endif()
  endif()
endfunction()

function(opus_supports_cpu_detection RUNTIME_CPU_CAPABILITY_DETECTION)
  set(RUNTIME_CPU_CAPABILITY_DETECTION 0 PARENT_SCOPE)
  if(OPUS_CPU_X86 OR OPUS_CPU_X64)
    if(MSVC)
      check_include_file(intrin.h HAVE_INTRIN_H)
      if(HAVE_INTRIN_H)
        # if intrin.h is available we assume __cpuid is there
        set(RUNTIME_CPU_CAPABILITY_DETECTION 1 PARENT_SCOPE)
      endif()
    else()
      include(CFeatureCheck)
      c_feature_check(CPU_INFO_BY_ASM)
      set(CPU_INFO_BY_ASM_SUPPORTED ${CPU_INFO_BY_ASM_SUPPORTED} PARENT_SCOPE)
      check_include_file(cpuid.h HAVE_CPUID_H)
      if(HAVE_CPUID_H)
        c_feature_check(CPU_INFO_BY_C)
        set(CPU_INFO_BY_C_SUPPORTED ${CPU_INFO_BY_C_SUPPORTED} PARENT_SCOPE)
      endif()
      if(CPU_INFO_BY_ASM_SUPPORTED OR CPU_INFO_BY_C_SUPPORTED)
        set(RUNTIME_CPU_CAPABILITY_DETECTION 1 PARENT_SCOPE)
      endif()
    endif()
  elseif(OPUS_CPU_ARM)
    # ARM cpu detection is implemented for Windows and anything
    # using a Linux kernel (such as Android).
    if (CMAKE_SYSTEM_NAME MATCHES "(Windows|Linux|Android)")
      set(RUNTIME_CPU_CAPABILITY_DETECTION 1 PARENT_SCOPE)
    endif ()
  else()
    set(RUNTIME_CPU_CAPABILITY_DETECTION 0 PARENT_SCOPE)
  endif()
endfunction()

function(add_sources_group target group)
  target_sources(${target} PRIVATE ${ARGN})
  source_group(${group} FILES ${ARGN})
endfunction()

function(get_opus_sources SOURCE_GROUP MAKE_FILE SOURCES)
  # read file, each item in list is one group
  file(STRINGS ${MAKE_FILE} opus_sources)

  # add wildcard for regex match
  string(CONCAT SOURCE_GROUP ${SOURCE_GROUP} ".*$")

  # find group
  foreach(val IN LISTS opus_sources)
    if(val MATCHES ${SOURCE_GROUP})
      list(LENGTH val list_length)
      if(${list_length} EQUAL 1)
        # for tests split by '=' and clean up the rest into a list
        string(FIND ${val} "=" index)
        math(EXPR index "${index} + 1")
        string(SUBSTRING ${val}
                         ${index}
                         -1
                         sources)
        string(REPLACE " "
                       ";"
                       sources
                       ${sources})
      else()
        # discard the group
        list(REMOVE_AT val 0)
        set(sources ${val})
      endif()
      break()
    endif()
  endforeach()

  list(LENGTH sources list_length)
  if(${list_length} LESS 1)
    message(
      FATAL_ERROR
        "No files parsed successfully from ${SOURCE_GROUP} in ${MAKE_FILE}")
  endif()

  # remove trailing whitespaces
  set(list_var "")
  foreach(source ${sources})
    string(STRIP "${source}" source)
    list(APPEND list_var "${source}")
  endforeach()

  set(${SOURCES} ${list_var} PARENT_SCOPE)
endfunction()
