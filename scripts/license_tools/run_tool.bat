
@echo off
setlocal enabledelayedexpansion

:: 获取脚本所在目录
set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

:: 检查Python依赖
echo 检查Python依赖...
pip install -r "%SCRIPT_DIR%\requirements.txt"

:: 启动GUI工具
echo 启动ESP32生产固件烧录工具...
python "%SCRIPT_DIR%\esp32_production_tool.py"
