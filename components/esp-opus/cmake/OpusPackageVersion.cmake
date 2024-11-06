if(__opus_version)
  return()
endif()
set(__opus_version INCLUDED)

function(get_package_version PACKAGE_VERSION PROJECT_VERSION)

  find_package(Git)
  if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE}
                    --git-dir=${CMAKE_CURRENT_LIST_DIR}/.git describe
                    --tags --match "v*" OUTPUT_VARIABLE OPUS_PACKAGE_VERSION)
    if(OPUS_PACKAGE_VERSION)
      string(STRIP ${OPUS_PACKAGE_VERSION}, OPUS_PACKAGE_VERSION)
      string(REPLACE \n
                     ""
                     OPUS_PACKAGE_VERSION
                     ${OPUS_PACKAGE_VERSION})
      string(REPLACE ,
                     ""
                     OPUS_PACKAGE_VERSION
                     ${OPUS_PACKAGE_VERSION})

      string(SUBSTRING ${OPUS_PACKAGE_VERSION}
                       1
                       -1
                       OPUS_PACKAGE_VERSION)
      message(STATUS "Opus package version from git repo: ${OPUS_PACKAGE_VERSION}")
    endif()

  elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/package_version"
         AND NOT OPUS_PACKAGE_VERSION)
    # Not a git repo, lets' try to parse it from package_version file if exists
    file(STRINGS package_version OPUS_PACKAGE_VERSION
         LIMIT_COUNT 1
         REGEX "PACKAGE_VERSION=")
    string(REPLACE "PACKAGE_VERSION="
                   ""
                   OPUS_PACKAGE_VERSION
                   ${OPUS_PACKAGE_VERSION})
    string(REPLACE "\""
                   ""
                   OPUS_PACKAGE_VERSION
                   ${OPUS_PACKAGE_VERSION})
    # In case we have a unknown dist here we just replace it with 0
    string(REPLACE "unknown"
                   "0"
                   OPUS_PACKAGE_VERSION
                   ${OPUS_PACKAGE_VERSION})
      message(STATUS "Opus package version from package_version file: ${OPUS_PACKAGE_VERSION}")
  endif()

  if(OPUS_PACKAGE_VERSION)
    string(REGEX
      REPLACE "^([0-9]+.[0-9]+\\.?([0-9]+)?).*"
               "\\1"
               OPUS_PROJECT_VERSION
               ${OPUS_PACKAGE_VERSION})
  else()
    # fail to parse version from git and package version
    message(WARNING "Could not get package version.")
    set(OPUS_PACKAGE_VERSION 0)
    set(OPUS_PROJECT_VERSION 0)
  endif()

  message(STATUS "Opus project version: ${OPUS_PROJECT_VERSION}")

  set(PACKAGE_VERSION ${OPUS_PACKAGE_VERSION} PARENT_SCOPE)
  set(PROJECT_VERSION ${OPUS_PROJECT_VERSION} PARENT_SCOPE)
endfunction()
