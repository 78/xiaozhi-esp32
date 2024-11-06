if(NOT EXISTS ${TEST_EXECUTABLE})
    message(FATAL_ERROR "Error could not find ${TEST_EXECUTABLE}, ensure that you built the test binary")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android")

  # support to run plain old binary on android devices
  # requires android debug bridge to be installed

  find_program(adb_executable adb)
  if(NOT adb_executable)
    message(FATAL_ERROR "Error could not find adb")
  endif()

  # check if any device emulator is attached
  execute_process(COMMAND ${adb_executable} shell echo RESULT_VARIABLE CMD_RESULT)
  if(CMD_RESULT)
    message(FATAL_ERROR "Error adb: no devices/emulators found")
  endif()

  # push binary
  set(android_path /data/local/tmp)
  execute_process(COMMAND ${adb_executable} push ${TEST_EXECUTABLE} ${android_path} RESULT_VARIABLE CMD_RESULT)
  if(CMD_RESULT)
    message(FATAL_ERROR "Error running ${adb_executable} push ${TEST_EXECUTABLE} ${android_path} failed with result ${CMD_RESULT}")
  endif()

  # set permissions
  get_filename_component(test_executable ${TEST_EXECUTABLE} NAME)
  set(test_executable_on_android /data/local/tmp/${test_executable})
  execute_process(COMMAND ${adb_executable} shell chmod 555 ${test_executable_on_android} RESULT_VARIABLE CMD_RESULT)
  if(CMD_RESULT)
    message(FATAL_ERROR "Error running ${adb_executable} shell chmod 555 ${test_executable_on_android} failed with result ${CMD_RESULT}")
  endif()

  # run executable
  execute_process(COMMAND ${adb_executable} shell ${test_executable_on_android} RESULT_VARIABLE CMD_RESULT)
  if(CMD_RESULT)
    message(FATAL_ERROR "Error running ${adb_executable} shell ${test_executable_on_android} failed with result ${CMD_RESULT}")
  endif()

  # clean up binary
  execute_process(COMMAND ${adb_executable} shell rm ${test_executable_on_android} RESULT_VARIABLE CMD_RESULT)
  if(CMD_RESULT)
    message(FATAL_ERROR "Error running ${adb_executable} shell rm ${test_executable_on_android} failed with result ${CMD_RESULT}")
  endif()

elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # CTest doesn't support iOS

  message(FATAL_ERROR "Error CTest is not supported on iOS")

else()
  # for other platforms just execute test binary on host

  execute_process(COMMAND ${TEST_EXECUTABLE} RESULT_VARIABLE CMD_RESULT)
  if(CMD_RESULT)
    message(FATAL_ERROR "Error running ${TEST_EXECUTABLE} failed with result ${CMD_RESULT}")
  endif()

endif()