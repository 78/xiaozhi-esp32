@echo off
REM Run this to set up the build system: configure, makefiles, etc.

setlocal enabledelayedexpansion

REM Parse the real autogen.sh script for version
for /F "tokens=2 delims= " %%A in ('findstr "dnn/download_model.sh" autogen.sh') do (
    set "model=%%A"
)

call dnn\download_model.bat %model%

echo Updating build configuration files, please wait....
