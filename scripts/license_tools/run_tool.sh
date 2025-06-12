#!/bin/bash

# ESP32生产固件烧录工具启动脚本

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 检查是否安装了所需依赖
echo "检查Python依赖..."
pip3 install -r "$SCRIPT_DIR/requirements.txt"

# 启动GUI工具
echo "启动ESP32生产固件烧录工具..."
python3 "$SCRIPT_DIR/esp32_production_tool.py" 