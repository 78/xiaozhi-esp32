
@echo off
setlocal enabledelayedexpansion

:: ��ȡ�ű�����Ŀ¼
set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

:: ���Python����
echo ���Python����...
pip install -r "%SCRIPT_DIR%\requirements.txt"

:: ����GUI����
echo ����ESP32�����̼���¼����...
python "%SCRIPT_DIR%\esp32_production_tool.py"
