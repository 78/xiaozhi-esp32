# - Compile and run code to check for C features
#
# This functions compiles a source file under the `cmake` folder
# and adds the corresponding `HAVE_[FILENAME]` flag to the CMake
# environment
#
#  c_feature_check(<FLAG> [<VARIANT>])
#
# - Example
#
# include(CFeatureCheck)
# c_feature_check(VLA)

if(__c_feature_check)
  return()
endif()
set(__c_feature_check INCLUDED)

function(c_feature_check FILE)
  string(TOLOWER ${FILE} FILE)
  string(TOUPPER ${FILE} VAR)
  string(TOUPPER "${VAR}_SUPPORTED" FEATURE)
  if (DEFINED ${VAR}_SUPPORTED)
    set(${VAR}_SUPPORTED 1 PARENT_SCOPE)
    return()
  endif()

  if (NOT DEFINED COMPILE_${FEATURE})
      message(STATUS "Performing Test ${FEATURE}")
      try_compile(COMPILE_${FEATURE} ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/cmake/${FILE}.c)
  endif()

  if(COMPILE_${FEATURE})
    message(STATUS "Performing Test ${FEATURE} -- success")
    set(${VAR}_SUPPORTED 1 PARENT_SCOPE)
  else()
    message(STATUS "Performing Test ${FEATURE} -- failed to compile")
  endif()
endfunction()
