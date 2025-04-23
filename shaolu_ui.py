#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import json
import re
import requests
import argparse
import logging
from pathlib import Path
import platform
import socket
from requests.exceptions import RequestException, Timeout, ConnectionError
import traceback

# 引入PySide6所需模块
try:
    from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
                                QPushButton, QLabel, QComboBox, QLineEdit, QTextEdit, 
                                QGroupBox, QFormLayout, QProgressBar, QMessageBox, QFileDialog,
                                QSplitter, QFrame)
    from PySide6.QtCore import Qt, QThread, Signal, Slot, QObject, QTimer
    from PySide6.QtGui import QFont, QIcon, QTextCursor, QColor, QPalette
    HAS_PYSIDE6 = True
except ImportError:
    HAS_PYSIDE6 = False

# 设置日志
logging.basicConfig(level=logging.INFO, 
                   format='%(asctime)s - %(levelname)s - %(message)s',
                   handlers=[logging.StreamHandler(), 
                             logging.FileHandler("shaolu_log.txt", encoding="utf-8")])
logger = logging.getLogger("shaolu")

try:
    import serial.tools.list_ports
except ImportError:
    logger.error("缺少依赖: pyserial库未安装")
    logger.info("请安装pyserial库: pip install pyserial")
    sys.exit(1)

# 基本配置
API_URL = "https://xiaoqiao-api.oaibro.com/admin-api/xiaoqiao/admin/device/add"  # 设备入库API地址
DEFAULT_WEBSOCKET_URL = "ws://113.45.139.160:8896/ws/device"  # 您的WebSocket服务器地址
MAX_RETRIES = 3  # 重试次数
TIMEOUT = 10  # 超时时间(秒)

# 定义日志批处理大小
LOG_BATCH_SIZE = 100 # 每 20 行发送一次更新

def is_valid_mac(mac):
    """验证MAC地址格式是否有效"""
    mac_pattern = re.compile(r'^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$')
    return bool(mac_pattern.match(mac))

def check_internet_connection():
    """检查互联网连接"""
    try:
        # 尝试连接到一个可靠的服务
        socket.create_connection(("www.baidu.com", 80), timeout=3)
        return True
    except OSError:
        return False

def check_esp_idf_installed():
    """检查ESP-IDF开发环境是否已安装"""
    logger.info("检查ESP-IDF开发环境...")
    
    # 检查方法1: 查找环境变量
    idf_path_env = os.environ.get("IDF_PATH")
    if idf_path_env and os.path.exists(idf_path_env):
        logger.info(f"检测到ESP-IDF环境变量: {idf_path_env}")
        return True, idf_path_env
    
    # 检查方法2: 检查常见安装路径
    system_name = platform.system()
    if system_name == "Windows":
        common_paths = [
            "C:\\Users\\1\\esp\\v5.3.2\\esp-idf",
            "C:\\esp\\esp-idf",
            os.path.expanduser("~/esp/esp-idf"),
            "C:\\Espressif\\frameworks\\esp-idf-v5.3.2",
            # 增加更多可能的路径
            "C:\\Espressif\\frameworks\\esp-idf-v5.0",
            "C:\\Espressif\\esp-idf"
        ]
    else:  # Linux/MacOS
        common_paths = [
            os.path.expanduser("~/esp/esp-idf"),
            "/opt/esp/esp-idf",
            "/usr/local/esp/esp-idf"
        ]
    
    for path in common_paths:
        if os.path.exists(path) and os.path.isdir(path):
            logger.info(f"检测到ESP-IDF安装路径: {path}")
            return True, path
    
    # 检查方法3: 尝试运行idf.py命令
    try:
        result = subprocess.run("idf.py --version", shell=True, capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            logger.info(f"检测到ESP-IDF命令可用: {result.stdout.strip()}")
            # 尝试从输出中获取路径
            match = re.search(r'IDF_PATH=(.*?)(\s|$)', result.stdout)
            path = match.group(1) if match else None
            return True, path
    except (subprocess.SubprocessError, subprocess.TimeoutExpired) as e:
        logger.debug(f"无法执行idf.py命令: {e}")
    except Exception as e:
        logger.debug(f"检查idf.py时发生异常: {e}")
    
    # 未检测到ESP-IDF
    logger.error("未检测到ESP-IDF开发环境!")
    logger.info("\n请按照以下步骤安装ESP-IDF环境:")
    logger.info("1. 访问: https://dl.espressif.com/dl/esp-idf/")
    logger.info("2. 下载并安装ESP-IDF v5.3.2或更高版本")
    logger.info("3. 按照安装向导完成安装")
    logger.info("4. 安装完成后重新运行此脚本\n")
    return False, None

def detect_ports():
    """自动检测可能的ESP32设备串口"""
    ports = []
    
    try:
        # 列出所有可用串口
        available_ports = list(serial.tools.list_ports.comports())
        
        # 根据常见ESP32设备的特征筛选串口
        for port in available_ports:
            # 常见ESP32设备的USB转串口芯片厂商ID或描述
            if any(keyword in port.description.lower() for keyword in ['cp210', 'ch340', 'ftdi', 'usb serial', 'silicon labs']):
                ports.append(port.device)
        
        if not ports and available_ports:
            # 如果没有找到匹配的端口但有其他端口，全部添加
            ports = [port.device for port in available_ports]
        
        if not ports:
            logger.warning("未检测到任何串口设备")
        else:
            logger.info(f"检测到以下可能的串口: {', '.join(ports)}")
            
    except Exception as e:
        logger.error(f"检测串口时发生错误: {e}")
        logger.debug(traceback.format_exc())
    
    return ports

def select_port(ports, auto_select=True):
    """选择一个串口，如果auto_select为True，则自动选择第一个"""
    if not ports:
        logger.error("未找到可用串口设备")
        return None
    
    if len(ports) == 1 or auto_select:
        selected_port = ports[0]
        logger.info(f"自动选择串口: {selected_port}")
        return selected_port
    
    # 让用户选择串口
    logger.info("检测到多个可能的串口:")
    for i, port in enumerate(ports):
        logger.info(f"{i+1}. {port}")
    
    max_attempts = 3
    for attempt in range(max_attempts):
        try:
            choice = input(f"请选择ESP32连接的串口 [1-{len(ports)}]: ")
            if not choice.strip():
                logger.warning("输入为空，请重新输入")
                continue
                
            choice_num = int(choice)
            if 1 <= choice_num <= len(ports):
                return ports[choice_num-1]
            else:
                logger.warning(f"选择超出范围，请输入1到{len(ports)}之间的数字")
        except ValueError:
            logger.warning("请输入有效的数字")
        except KeyboardInterrupt:
            logger.info("\n用户取消选择")
            return None
            
        if attempt == max_attempts - 1:
            logger.warning("多次输入无效，默认选择第一个串口")
            return ports[0]

def test_port_connection(port, timeout=5):
    """测试串口连接是否正常"""
    logger.info(f"测试串口 {port} 连接...")
    try:
        ser = serial.Serial(port, 115200, timeout=timeout)
        ser.close()
        logger.info(f"串口 {port} 连接正常")
        return True
    except serial.SerialException as e:
        logger.error(f"无法连接到串口 {port}: {e}")
        if "Access is denied" in str(e):
            logger.info("可能原因: 该串口被其他程序占用，请关闭其他使用此串口的程序")
        elif "could not open port" in str(e):
            logger.info("可能原因: 串口不存在或权限不足")
        return False
    except Exception as e:
        logger.error(f"测试串口连接时发生未知错误: {e}")
        return False

def get_device_mac(port):
    """获取设备的MAC地址"""
    for attempt in range(MAX_RETRIES):
        try:
            logger.info(f"尝试获取设备MAC地址 (尝试 {attempt+1}/{MAX_RETRIES})...")
            
            # 首先测试串口连接
            if not test_port_connection(port):
                if attempt < MAX_RETRIES - 1:
                    logger.info("等待5秒后重试...")
                    time.sleep(5)
                    continue
                else:
                    return None
            
            # 检查esptool.py是否存在
            esptool_path = "esptool.py"
            
            # 尝试在ESP-IDF环境中查找esptool
            esp_idf_path = os.environ.get("IDF_PATH")
            if esp_idf_path:
                possible_paths = [
                    os.path.join(esp_idf_path, "components", "esptool_py", "esptool", "esptool.py"),
                    os.path.join(esp_idf_path, "tools", "esptool.py")
                ]
                for path in possible_paths:
                    if os.path.exists(path):
                        esptool_path = path
                        break
            
            logger.info(f"使用esptool路径: {esptool_path}")
            
            # 尝试使用Python调用esptool模块
            cmd = [sys.executable, "-m", "esptool", "--port", port, "read_mac"]
            logger.info(f"执行命令: {' '.join(cmd)}")
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
            if result.returncode != 0:
                logger.warning(f"使用Python模块运行失败: {result.stderr}")
                
                # 尝试直接调用esptool.py
                cmd = [esptool_path, "--port", port, "read_mac"]
                logger.info(f"尝试直接调用: {' '.join(cmd)}")
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
                
                if result.returncode != 0:
                    logger.error(f"获取MAC地址失败: {result.stderr}")
                    
                    if attempt < MAX_RETRIES - 1:
                        logger.info("等待5秒后重试...")
                        time.sleep(5)
                        continue
                    else:
                        logger.info("请确保安装了esptool: pip install esptool")
                        logger.info("如果您使用ESP-IDF，请确保已执行ESP-IDF环境设置脚本")
                        logger.info("Windows: %IDF_PATH%\\export.bat")
                        logger.info("Linux/macOS: source $IDF_PATH/export.sh")
                        return None
            
            # 从输出中解析MAC地址
            logger.debug(f"命令输出: {result.stdout}")
            mac_match = re.search(r'MAC:\s+([0-9a-f:]{17})', result.stdout)
            if not mac_match:
                logger.error(f"无法从输出中解析MAC地址")
                if attempt < MAX_RETRIES - 1:
                    logger.info("等待5秒后重试...")
                    time.sleep(5)
                    continue
                else:
                    return None
            
            mac_address = mac_match.group(1)
            if not is_valid_mac(mac_address):
                logger.error(f"获取到的MAC地址格式无效: {mac_address}")
                if attempt < MAX_RETRIES - 1:
                    continue
                else:
                    return None
                
            logger.info(f"获取到设备MAC地址: {mac_address}")
            return mac_address
            
        except subprocess.TimeoutExpired:
            logger.error("执行esptool命令超时")
        except Exception as e:
            logger.error(f"获取MAC地址时发生错误: {str(e)}")
            logger.debug(traceback.format_exc())
        
        if attempt < MAX_RETRIES - 1:
            logger.info(f"等待5秒后重试 ({attempt+1}/{MAX_RETRIES})...")
            time.sleep(5)
        else:
            logger.error("多次尝试获取MAC地址均失败")
    
    return None

def register_device(mac_address):
    """通过API注册设备，获取token和clientId"""
    if not check_internet_connection():
        logger.error("无法连接到互联网，设备注册失败")
        return None, None, None
        
    for attempt in range(MAX_RETRIES):
        try:
            logger.info(f"正在通过API注册设备 (MAC: {mac_address})... 尝试 {attempt+1}/{MAX_RETRIES}")
            headers = {
                "Content-Type": "application/json"
            }
            data = {
                "clientType": "esp32",
                "deviceId": mac_address,
                "deviceName": f"ESP32设备-{mac_address.replace(':', '')[-4:]}",
                "deviceVersion": "1.0.0"
            }
            
            logger.info(f"发送请求到: {API_URL}")
            logger.debug(f"请求数据: {json.dumps(data, ensure_ascii=False)}")
            
            response = requests.post(API_URL, headers=headers, json=data, timeout=TIMEOUT)
            logger.info(f"API响应状态码: {response.status_code}")
            logger.debug(f"API响应内容: {response.text}")
            
            response.raise_for_status()
            
            result = response.json()
            if result.get("code") != 0:
                logger.error(f"设备入库失败: {result.get('msg', '未知错误')}")
                if attempt < MAX_RETRIES - 1:
                    logger.info(f"等待3秒后重试...")
                    time.sleep(3)
                    continue
                return None, None, None
                
            token = result.get("data", {}).get("apiKey")
            client_id = result.get("data", {}).get("clientId")
            bind_key = result.get("data", {}).get("bindKey")
            
            if not token:
                logger.error(f"API响应中没有apiKey: {result}")
                if attempt < MAX_RETRIES - 1:
                    continue
                return None, None, None
                
            if not client_id:
                logger.warning(f"API响应中没有clientId，使用MAC地址代替")
                client_id = mac_address.replace(':', '')  # 降级到默认行为
            
            logger.info(f"设备入库成功:")
            logger.info(f"- Token: {token}")
            logger.info(f"- Client ID: {client_id}")
            if bind_key:
                logger.info(f"- 绑定码: {bind_key}")
            
            return token, client_id, bind_key
            
        except Timeout:
            logger.error(f"API请求超时 (尝试 {attempt+1}/{MAX_RETRIES})")
        except ConnectionError:
            logger.error(f"API连接错误 (尝试 {attempt+1}/{MAX_RETRIES})")
        except RequestException as e:
            logger.error(f"API请求异常: {e} (尝试 {attempt+1}/{MAX_RETRIES})")
        except json.JSONDecodeError:
            logger.error(f"API响应格式无效 (尝试 {attempt+1}/{MAX_RETRIES})")
        except Exception as e:
            logger.error(f"设备入库失败: {e} (尝试 {attempt+1}/{MAX_RETRIES})")
            logger.debug(traceback.format_exc())
        
        if attempt < MAX_RETRIES - 1:
            wait_time = 3 * (attempt + 1)  # 指数退避
            logger.info(f"等待{wait_time}秒后重试...")
            time.sleep(wait_time)
    
    logger.error("多次尝试设备入库均失败")
    return None, None, None

def update_config(token, websocket_url, client_id=None):
    """直接更新sdkconfig文件中的设置"""
    try:
        logger.info("正在更新sdkconfig配置...")
        
        sdkconfig_path = "sdkconfig"
        if not os.path.exists(sdkconfig_path):
            logger.error(f"找不到sdkconfig文件: {sdkconfig_path}")
            logger.info("请确保在项目根目录运行此脚本")
            return False
            
        # 备份原始文件
        backup_path = f"{sdkconfig_path}.bak"
        try:
            with open(sdkconfig_path, "r", encoding="utf-8") as src:
                with open(backup_path, "w", encoding="utf-8") as dst:
                    dst.write(src.read())
            logger.info(f"已创建配置文件备份: {backup_path}")
        except Exception as e:
            logger.warning(f"创建配置文件备份失败: {e}")
        
        # 读取sdkconfig文件内容
        try:
            with open(sdkconfig_path, "r", encoding="utf-8") as f:
                content = f.read()
        except UnicodeDecodeError:
            logger.warning("UTF-8解码失败，尝试其他编码...")
            with open(sdkconfig_path, "r", encoding="latin-1") as f:
                content = f.read()
        
        # 使用正则表达式替换配置项
        import re
        
        # 替换WebSocket URL
        content = re.sub(r'CONFIG_WEBSOCKET_URL="[^"]*"',
                        f'CONFIG_WEBSOCKET_URL="{websocket_url}"',
                        content)
        
        # 如果没有找到WebSocket URL配置项，则添加它
        if 'CONFIG_WEBSOCKET_URL=' not in content:
            content += f'\nCONFIG_WEBSOCKET_URL="{websocket_url}"\n'
        
        # 替换Access Token
        content = re.sub(r'CONFIG_WEBSOCKET_ACCESS_TOKEN="[^"]*"',
                        f'CONFIG_WEBSOCKET_ACCESS_TOKEN="{token}"',
                        content)
                        
        # 如果没有找到Access Token配置项，则添加它
        if 'CONFIG_WEBSOCKET_ACCESS_TOKEN=' not in content:
            content += f'\nCONFIG_WEBSOCKET_ACCESS_TOKEN="{token}"\n'
        
        # 添加或替换 Client ID (如果提供)
        if client_id:
            if "CONFIG_WEBSOCKET_CLIENT_ID=" in content:
                content = re.sub(r'CONFIG_WEBSOCKET_CLIENT_ID="[^"]*"',
                                f'CONFIG_WEBSOCKET_CLIENT_ID="{client_id}"',
                                content)
            else:
                # 如果配置中不存在该项，在适当位置添加它
                insert_pos = content.find("CONFIG_WEBSOCKET_ACCESS_TOKEN=")
                if insert_pos > 0:
                    line_end = content.find("\n", insert_pos)
                    content = content[:line_end+1] + f'CONFIG_WEBSOCKET_CLIENT_ID="{client_id}"\n' + content[line_end+1:]
                else:
                    content += f'\nCONFIG_WEBSOCKET_CLIENT_ID="{client_id}"\n'
        
        # 确保WebSocket模式启用
        content = re.sub(r'# CONFIG_CONNECTION_TYPE_WEBSOCKET is not set',
                        'CONFIG_CONNECTION_TYPE_WEBSOCKET=y',
                        content)
                        
        if 'CONFIG_CONNECTION_TYPE_WEBSOCKET=y' not in content:
            content += '\nCONFIG_CONNECTION_TYPE_WEBSOCKET=y\n'
        
        if 'CONFIG_CONNECTION_TYPE_MQTT_UDP=y' in content:
            content = content.replace('CONFIG_CONNECTION_TYPE_MQTT_UDP=y', 
                                    '# CONFIG_CONNECTION_TYPE_MQTT_UDP is not set')
        
        # 写回sdkconfig文件
        try:
            with open(sdkconfig_path, "w", encoding="utf-8") as f:
                f.write(content)
        except Exception as e:
            logger.error(f"写入配置文件时发生错误: {e}")
            logger.info("尝试恢复备份文件...")
            
            try:
                if os.path.exists(backup_path):
                    with open(backup_path, "r", encoding="utf-8") as src:
                        with open(sdkconfig_path, "w", encoding="utf-8") as dst:
                            dst.write(src.read())
                    logger.info("已恢复原始配置文件")
                    return False
            except Exception as restore_error:
                logger.error(f"恢复备份文件失败: {restore_error}")
            return False
        
        logger.info(f"已更新sdkconfig:")
        logger.info(f"- WebSocket URL: {websocket_url}")
        logger.info(f"- Access Token: {token}")
        if client_id:
            logger.info(f"- Client ID: {client_id}")
        
        return True
    except Exception as e:
        logger.error(f"更新配置时发生错误: {str(e)}")
        logger.debug(traceback.format_exc())
        return False

def build_firmware(idf_path=None, skip_clean=False, progress_callback=None):
    try:
        logger.info("正在编译固件...")

        # 如果没有提供ESP-IDF路径，尝试获取
        if not idf_path:
            idf_path_env = os.environ.get("IDF_PATH")
            if idf_path_env:
                idf_path = idf_path_env
            else:
                # 尝试检测ESP-IDF安装
                is_installed, detected_path = check_esp_idf_installed()
                if is_installed and detected_path:
                    idf_path = detected_path
                else:
                    logger.error("无法确定ESP-IDF路径，请手动提供")
                    return False

        logger.info(f"使用ESP-IDF路径: {idf_path}")

        # 根据操作系统选择正确的激活脚本
        system_name = platform.system()
        if system_name == "Windows":
            export_script = os.path.join(idf_path, "export.bat")
            # 确保路径被正确引用，特别是包含空格时
            activate_cmd = f"call \"{export_script}\""
        else:  # Linux/MacOS
            export_script = os.path.join(idf_path, "export.sh")
            activate_cmd = f"source \"{export_script}\""

        # 先执行清理命令
        if not skip_clean:
            clean_cmd = f"{activate_cmd} && idf.py fullclean"
            logger.info(f"执行命令: {clean_cmd}")

            # 简化子进程处理
            process_clean = subprocess.Popen(clean_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1, universal_newlines=True)
            for line in iter(process_clean.stdout.readline, ''):
                logger.info(line.strip())
            process_clean.wait()
            if process_clean.returncode != 0:
                logger.warning(f"清理项目返回非零值: {process_clean.returncode}，但将继续编译")
        else:
             logger.info("跳过清理步骤")

        # 修改编译命令部分
        build_cmd = f"{activate_cmd} && idf.py build"
        logger.info(f"执行命令: {build_cmd}")

        process_build = subprocess.Popen(build_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1, universal_newlines=True, encoding='utf-8', errors='replace')

        # 修改输出处理循环
        for line in iter(process_build.stdout.readline, ''):
            if progress_callback:
                progress_callback(line)
            else:
                logger.debug(line.strip())

            # 检查是否有错误信息
            if "error:" in line.lower() or "fail" in line.lower():
                 logger.error(line.strip())

        process_build.wait()
        if process_build.returncode != 0:
            logger.error(f"编译固件失败，返回码: {process_build.returncode}")
            return False

        logger.info("固件编译成功")
        return True
    except Exception as e:
        logger.error(f"编译固件时发生错误: {str(e)}")
        logger.debug(traceback.format_exc())
        return False

def flash_firmware(port, idf_path=None, progress_callback=None):
    """烧录固件"""
    try:
        logger.info(f"正在烧录固件到设备 (端口: {port})...")
        
        # 首先测试串口连接
        if not test_port_connection(port):
            logger.error("串口连接测试失败，无法继续烧录")
            return False
        
        # 如果没有提供ESP-IDF路径，尝试获取
        if not idf_path:
            idf_path_env = os.environ.get("IDF_PATH")
            if idf_path_env:
                idf_path = idf_path_env
            else:
                # 尝试检测ESP-IDF安装
                is_installed, detected_path = check_esp_idf_installed()
                if is_installed and detected_path:
                    idf_path = detected_path
                else:
                    logger.error("无法确定ESP-IDF路径，请手动提供")
                    return False
        
        # 根据操作系统选择正确的激活脚本
        system_name = platform.system()
        if system_name == "Windows":
            export_script = os.path.join(idf_path, "export.bat")
            activate_cmd = f"call \"{export_script}\" && "
        else:  # Linux/MacOS
            export_script = os.path.join(idf_path, "export.sh")
            activate_cmd = f"source \"{export_script}\" && "
        
        # 检查固件文件是否存在
        build_dir = "build"
        if not os.path.exists(build_dir) or not os.path.isdir(build_dir):
            logger.error(f"找不到编译输出目录: {build_dir}")
            logger.info("请先成功编译固件")
            return False
        
        # 执行export.bat激活环境并烧录
        flash_cmd = f"{activate_cmd}idf.py -p {port} flash"
        logger.info(f"执行命令: {flash_cmd}")
        
        max_attempts = 3
        for attempt in range(max_attempts):
            try:
                logger.info(f"烧录尝试 {attempt+1}/{max_attempts}")
                
                process = subprocess.Popen(flash_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1, universal_newlines=True, encoding='utf-8', errors='replace')
                
                for line in iter(process.stdout.readline, ''):
                    if progress_callback:
                        progress_callback(line)
                    else:
                        logger.debug(line.strip())
                
                    if "error" in line.lower() or "fail" in line.lower():
                        logger.error(line.strip())
                
                process.wait(timeout=300)  # 5分钟超时
                if process.returncode == 0:
                    logger.info("固件烧录成功")
                    return True
                else:
                    logger.error(f"烧录固件失败，返回码: {process.returncode}")
                    
                    if attempt < max_attempts - 1:
                        logger.info("等待10秒后重试...")
                        time.sleep(10)
                    else:
                        logger.error("多次尝试烧录均失败")
                        return False
                        
            except subprocess.TimeoutExpired:
                logger.error("烧录固件超时")
                process.kill()
                
                if attempt < max_attempts - 1:
                    logger.info("等待10秒后重试...")
                    time.sleep(10)
                else:
                    return False
            except Exception as e:
                logger.error(f"烧录过程中发生错误: {e}")
                
                if attempt < max_attempts - 1:
                    logger.info("等待10秒后重试...")
                    time.sleep(10)
                else:
                    return False
        
        return False
    except Exception as e:
        logger.error(f"烧录固件时发生错误: {str(e)}")
        logger.debug(traceback.format_exc())
        return False

def cleanup():
    """清理临时文件和资源"""
    try:
        # 例如删除备份文件等
        backup_path = "sdkconfig.bak"
        if os.path.exists(backup_path):
            os.remove(backup_path)
            logger.debug(f"已删除备份文件: {backup_path}")
    except Exception as e:
        logger.warning(f"清理资源时发生错误: {e}")

def check_project_valid():
    """检查当前目录是否是有效的ESP-IDF项目"""
    if not os.path.exists("CMakeLists.txt"):
        logger.error("当前目录不是有效的ESP-IDF项目，缺少CMakeLists.txt")
        return False
    return True

# 添加GUI相关代码
class LogRedirector(QObject):
    """将日志重定向到UI的工具类"""
    log_signal = Signal(str, int)  # 文本, 级别(日志级别)

    def __init__(self, logger_instance):
        super().__init__()
        self.logger = logger_instance
        self.handler = None
        self.setup_handler()

    def setup_handler(self):
        class SignalHandler(logging.Handler):
            def __init__(self, signal_emitter):
                super().__init__()
                self.signal_emitter = signal_emitter

            def emit(self, record):
                msg = self.format(record)
                self.signal_emitter.log_signal.emit(msg, record.levelno)

        self.handler = SignalHandler(self)
        self.handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))
        self.logger.addHandler(self.handler)

class WorkerThread(QThread):
    """处理后台任务的线程类"""
    finished = Signal(object, str)  # 结果和错误消息
    progress = Signal(int, str)  # 进度值和进度消息
    log_batch_signal = Signal(str) # <--- 新增：用于发送批量日志

    def __init__(self, function, *args, **kwargs):
        super().__init__()
        self.function = function
        self.args = args
        self.kwargs = kwargs
        self.result = None
        self.error = None
        self._stop_requested = False
        self._timeout_timer = QTimer()
        self._timeout_timer.setSingleShot(True)
        self._timeout_timer.timeout.connect(self.handle_timeout)
        
        # --- 新增：日志批处理相关 ---
        self._log_buffer = []
        self._line_count = 0
        # --- 结束新增 ---

    def handle_timeout(self):
        """处理线程执行超时情况"""
        logger.debug("线程执行超时，尝试强制终止")
        self.stop()
        if self.isRunning():
            self.terminate()

    def stop(self):
        """请求停止线程"""
        logger.debug("线程停止请求")
        self._stop_requested = True

    def safe_start(self, timeout=600000): # 增加默认超时时间到10分钟以适应编译
        """安全启动线程，带超时处理"""
        if timeout > 0:
            self._timeout_timer.start(timeout)
        self.start()

    # --- 新增：日志批处理方法 ---
    def _emit_log_batch(self):
        """发送缓存的日志批次"""
        if self._log_buffer:
            batch_text = "".join(self._log_buffer)
            self.log_batch_signal.emit(batch_text)
            self._log_buffer.clear()
            self._line_count = 0

    def _handle_progress_log(self, line):
        """处理单行日志，进行缓存和批量发送"""
        if self._stop_requested: # 如果已请求停止，不再处理日志
             return
        self._log_buffer.append(line)
        self._line_count += 1
        if self._line_count >= LOG_BATCH_SIZE:
            self._emit_log_batch()
    # --- 结束新增 ---

    def run(self):
        try:
            # 准备回调函数参数
            callback_kwargs = self.kwargs.copy()
            # 检查目标函数是否接受 progress_callback 参数
            # (更健壮的方式是使用 inspect 模块检查函数签名，这里简化处理)
            # 假设 build_firmware 和 flash_firmware 会接受这个回调
            if self.function.__name__ in ['build_firmware', 'flash_firmware']:
                 callback_kwargs['progress_callback'] = self._handle_progress_log

            # 执行实际函数
            self.result = self.function(*self.args, **callback_kwargs)

            # --- 修改：确保最后剩余的日志被发送 ---
            self._emit_log_batch()
            # --- 结束修改 ---

            if not self._stop_requested:
                self.finished.emit(self.result, "")
        except Exception as e:
             # --- 修改：确保异常发生时也发送剩余日志 ---
            self._emit_log_batch()
            # --- 结束修改 ---
            self.error = str(e)
            if not self._stop_requested:
                self.finished.emit(None, str(e))
            logger.error(f"线程执行失败: {e}")
            logger.debug(traceback.format_exc())
        finally:
            # 确保计时器停止
            self._timeout_timer.stop()

class ShaoluUI(QMainWindow):
    """主UI窗口"""
    def __init__(self):
        super().__init__()
        
        # 初始化线程列表（需要提前初始化）
        self._active_threads = []
        
        # 基础设置
        self.setWindowTitle("ESP32S3自动烧录工具")
        self.setMinimumSize(900, 650)
        self.idf_path = None
        self.mac_address = None
        self.token = None
        self.client_id = None
        self.bind_key = None
        
        # 创建UI组件
        self.init_ui()
        
        # 创建日志重定向器
        self.log_redirector = LogRedirector(logger)
        self.log_redirector.log_signal.connect(self.update_log)
        
        # 检查ESP-IDF
        self.check_esp_idf()
        
        # 初始刷新串口
        self.port_refresh()
    
    def init_ui(self):
        """初始化UI界面"""
        # 主布局
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QVBoxLayout(main_widget)
        main_layout.setContentsMargins(10, 10, 10, 10)
        main_layout.setSpacing(10)
        
        # 顶部标题
        title_label = QLabel("独众固件 自动烧录工具")
        title_font = QFont("微软雅黑", 16, QFont.Bold)
        title_label.setFont(title_font)
        title_label.setAlignment(Qt.AlignCenter)
        main_layout.addWidget(title_label)
        
        # 创建分割器以便调整各部分大小
        splitter = QSplitter(Qt.Vertical)
        main_layout.addWidget(splitter, 1)
        
        # 上部分：配置和操作区域
        top_widget = QWidget()
        top_layout = QVBoxLayout(top_widget)
        top_layout.setContentsMargins(0, 0, 0, 0)
        top_layout.setSpacing(10)
        
        # 串口选择区域
        self.create_port_selection_group(top_layout)
        
        # 配置区域
        self.create_config_group(top_layout)
        
        # 操作按钮区域
        self.create_actions_group(top_layout)
        
        # 进度条
        self.progress_bar = QProgressBar()
        self.progress_bar.setMinimum(0)
        self.progress_bar.setMaximum(100)
        self.progress_bar.setValue(0)
        self.progress_bar.setFormat("就绪")
        self.progress_bar.setAlignment(Qt.AlignCenter)
        self.progress_bar.setTextVisible(True)
        top_layout.addWidget(self.progress_bar)
        
        # 状态标签
        self.status_label = QLabel("准备就绪")
        self.status_label.setAlignment(Qt.AlignCenter)
        top_layout.addWidget(self.status_label)
        
        # 添加到分割器
        splitter.addWidget(top_widget)
        
        # 下部分：日志区域
        self.create_log_group(splitter)
        
        # 状态栏
        self.statusBar().showMessage("准备就绪")
    
    def create_port_selection_group(self, parent_layout):
        """创建串口选择区域"""
        group_box = QGroupBox("设备连接")
        layout = QHBoxLayout()
        
        # 串口选择
        layout.addWidget(QLabel("串口:"))
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(200)
        layout.addWidget(self.port_combo, 1)
        
        # 刷新按钮
        refresh_button = QPushButton("刷新")
        refresh_button.setIcon(QIcon.fromTheme("view-refresh"))
        refresh_button.clicked.connect(self.port_refresh)
        layout.addWidget(refresh_button)
        
        # MAC地址显示
        layout.addWidget(QLabel("MAC:"))
        self.mac_display = QLineEdit()
        self.mac_display.setReadOnly(True)
        self.mac_display.setPlaceholderText("未获取")
        layout.addWidget(self.mac_display, 1)
        
        # 获取MAC按钮
        self.get_mac_button = QPushButton("获取MAC")
        self.get_mac_button.clicked.connect(self.on_get_mac_clicked)
        layout.addWidget(self.get_mac_button)
        
        group_box.setLayout(layout)
        parent_layout.addWidget(group_box)
    
    def create_config_group(self, parent_layout):
        """创建配置区域"""
        group_box = QGroupBox("设备配置")
        layout = QFormLayout()
        
        # WebSocket URL
        self.websocket_url_edit = QLineEdit(DEFAULT_WEBSOCKET_URL)
        layout.addRow("WebSocket URL:", self.websocket_url_edit)
        
        # Token显示
        self.token_display = QLineEdit()
        self.token_display.setReadOnly(True)
        self.token_display.setPlaceholderText("未获取")
        layout.addRow("Access Token:", self.token_display)
        
        # Client ID显示
        self.client_id_display = QLineEdit()
        self.client_id_display.setReadOnly(True)
        self.client_id_display.setPlaceholderText("未获取")
        layout.addRow("Client ID:", self.client_id_display)
        
        # ESP-IDF路径显示
        self.idf_path_display = QLineEdit()
        self.idf_path_display.setReadOnly(True)
        self.idf_path_display.setPlaceholderText("未检测")
        layout.addRow("ESP-IDF路径:", self.idf_path_display)
        
        group_box.setLayout(layout)
        parent_layout.addWidget(group_box)
    
    def create_actions_group(self, parent_layout):
        """创建操作按钮区域"""
        group_box = QGroupBox("操作")
        layout = QVBoxLayout()
        
        # 上层操作按钮
        top_buttons = QHBoxLayout()
        
        self.register_button = QPushButton("注册设备")
        self.register_button.clicked.connect(self.on_register_clicked)
        top_buttons.addWidget(self.register_button)
        
        self.update_config_button = QPushButton("更新配置")
        self.update_config_button.clicked.connect(self.on_update_config_clicked)
        top_buttons.addWidget(self.update_config_button)
        
        self.build_button = QPushButton("编译固件")
        self.build_button.clicked.connect(self.on_build_clicked)
        top_buttons.addWidget(self.build_button)
        
        self.flash_button = QPushButton("烧录固件")
        self.flash_button.clicked.connect(self.on_flash_clicked)
        top_buttons.addWidget(self.flash_button)
        
        layout.addLayout(top_buttons)
        
        # 一键操作按钮
        self.all_in_one_button = QPushButton("一键完成全部流程")
        self.all_in_one_button.setMinimumHeight(40)
        font = self.all_in_one_button.font()
        font.setBold(True)
        self.all_in_one_button.setFont(font)
        self.all_in_one_button.clicked.connect(self.on_all_in_one_clicked)
        layout.addWidget(self.all_in_one_button)
        
        group_box.setLayout(layout)
        parent_layout.addWidget(group_box)
    
    def create_log_group(self, parent):
        """创建日志显示区域"""
        log_widget = QWidget()
        layout = QVBoxLayout(log_widget)
        layout.setContentsMargins(0, 0, 0, 0)
        
        # 日志标题
        log_header = QHBoxLayout()
        log_label = QLabel("日志输出")
        log_label.setFont(QFont("微软雅黑", 10, QFont.Bold))
        log_header.addWidget(log_label)
        
        # 清空日志按钮
        clear_button = QPushButton("清空")
        clear_button.setMaximumWidth(80)
        clear_button.clicked.connect(self.clear_log)
        log_header.addWidget(clear_button, 0, Qt.AlignRight)
        
        layout.addLayout(log_header)
        
        # 日志文本区域
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        font = QFont("Consolas", 9)
        self.log_text.setFont(font)
        self.log_text.setLineWrapMode(QTextEdit.NoWrap)
        layout.addWidget(self.log_text)
        
        parent.addWidget(log_widget)
    
    def check_esp_idf(self):
        """检查ESP-IDF环境"""
        self.progress_bar.setValue(10)
        self.progress_bar.setFormat("检查ESP-IDF环境...")
        self.status_label.setText("正在检查ESP-IDF环境...")
        
        self.worker = self.start_worker(check_esp_idf_installed, self.on_esp_idf_check_finished)
    
    @Slot(object, str)
    def on_esp_idf_check_finished(self, result, error):
        """ESP-IDF检查完成的处理"""
        if result:
            is_installed, idf_path = result
            if is_installed and idf_path:
                self.idf_path = idf_path
                self.idf_path_display.setText(idf_path)
                self.status_label.setText(f"ESP-IDF环境已检测: {idf_path}")
            else:
                self.status_label.setText("未检测到ESP-IDF环境")
                QMessageBox.warning(self, "环境检查", "未检测到ESP-IDF环境，部分功能可能不可用")
        else:
            self.status_label.setText("检查ESP-IDF环境失败")
            QMessageBox.warning(self, "环境检查", f"检查ESP-IDF环境时出错: {error}")
        
        self.progress_bar.setValue(0)
        self.progress_bar.setFormat("就绪")
    
    @Slot()
    def port_refresh(self):
        """刷新串口列表"""
        self.port_combo.clear()
        self.status_label.setText("正在检测串口...")
        
        self.worker = self.start_worker(detect_ports, self.on_port_refresh_finished)
    
    @Slot(object, str)
    def on_port_refresh_finished(self, ports, error):
        """串口刷新完成的处理"""
        if ports and isinstance(ports, list):
            self.port_combo.addItems(ports)
            self.status_label.setText(f"检测到 {len(ports)} 个串口")
            self.statusBar().showMessage(f"检测到 {len(ports)} 个串口")
        else:
            self.status_label.setText("未检测到可用串口")
            self.statusBar().showMessage("未检测到可用串口")
    
    @Slot(str, int)
    def update_log(self, message, level):
        """更新日志显示 (处理标准日志)"""
        # 设置不同级别的颜色
        if level >= logging.ERROR:
            color = "red"
        elif level >= logging.WARNING:
            color = "orange"
        elif level >= logging.INFO:
            color = "black"
        else:
            color = "gray"
            
        # 添加带颜色的文本
        self.log_text.moveCursor(QTextCursor.End)
        self.log_text.insertHtml(f"<span style='color:{color};'>{message}</span><br>")
        self.log_text.moveCursor(QTextCursor.End)
    
    @Slot()
    def clear_log(self):
        """清空日志区域"""
        self.log_text.clear()
    
    def get_selected_port(self):
        """获取选中的串口"""
        if self.port_combo.currentText():
            return self.port_combo.currentText()
        return None
    
    def set_ui_enabled(self, enabled):
        """设置UI是否可用"""
        self.get_mac_button.setEnabled(enabled)
        self.register_button.setEnabled(enabled)
        self.update_config_button.setEnabled(enabled)
        self.build_button.setEnabled(enabled)
        self.flash_button.setEnabled(enabled)
        self.all_in_one_button.setEnabled(enabled)
        self.port_combo.setEnabled(enabled)
        self.websocket_url_edit.setEnabled(enabled)
    
    @Slot()
    def on_get_mac_clicked(self):
        """获取MAC地址按钮点击事件"""
        port = self.get_selected_port()
        if not port:
            QMessageBox.warning(self, "错误", "请选择一个串口")
            return
            
        self.set_ui_enabled(False)
        self.status_label.setText("正在获取MAC地址...")
        self.progress_bar.setValue(20)
        self.progress_bar.setFormat("获取MAC地址...")
        
        # 正确传递回调函数和参数
        self.worker = self.start_worker(get_device_mac, self.on_get_mac_finished, port)
    
    @Slot(object, str)
    def on_get_mac_finished(self, mac_address, error):
        """获取MAC地址完成事件"""
        self.set_ui_enabled(True)
        self.progress_bar.setValue(0)
        self.progress_bar.setFormat("就绪")
        
        if mac_address:
            self.status_label.setText(f"MAC地址获取成功: {mac_address}")
            self.mac_address = mac_address
            self.mac_display.setText(mac_address)
        else:
            self.status_label.setText(f"MAC地址获取失败")
            QMessageBox.warning(self, "错误", f"无法获取MAC地址: {error}")
    
    @Slot()
    def on_register_clicked(self):
        """注册设备按钮点击事件"""
        if not self.mac_address:
            QMessageBox.warning(self, "错误", "请先获取MAC地址")
            return
            
        self.set_ui_enabled(False)
        self.status_label.setText("正在注册设备...")
        self.progress_bar.setValue(30)
        self.progress_bar.setFormat("注册设备...")
        
        # 正确传递参数 - 先传回调函数，然后是mac_address参数
        self.worker = self.start_worker(register_device, self.on_register_finished, self.mac_address)
    
    @Slot(object, str)
    def on_register_finished(self, result, error):
        """注册设备完成事件"""
        self.set_ui_enabled(True)
        self.progress_bar.setValue(0)
        self.progress_bar.setFormat("就绪")
        
        if result and isinstance(result, tuple) and len(result) >= 2:
            token, client_id = result[0], result[1]
            bind_key = result[2] if len(result) > 2 else None
            
            if token:
                self.token = token
                self.client_id = client_id
                self.bind_key = bind_key
                self.token_display.setText(token)
                self.client_id_display.setText(client_id if client_id else "使用MAC地址替代")
                self.status_label.setText("设备注册成功")
            else:
                self.status_label.setText("设备注册失败: 未获取到token")
                QMessageBox.warning(self, "错误", "设备注册失败: 未获取到token")
        else:
            self.status_label.setText(f"设备注册失败")
            QMessageBox.warning(self, "错误", f"设备注册失败: {error}")
    
    @Slot()
    def on_update_config_clicked(self):
        """更新配置按钮点击事件"""
        if not self.token:
            QMessageBox.warning(self, "错误", "请先注册设备获取Token")
            return
            
        websocket_url = self.websocket_url_edit.text()
        if not websocket_url:
            QMessageBox.warning(self, "错误", "WebSocket URL不能为空")
            return
            
        self.set_ui_enabled(False)
        self.status_label.setText("正在更新配置...")
        self.progress_bar.setValue(40)
        self.progress_bar.setFormat("更新配置...")
        
        # 创建工作线程
        self.worker = self.start_worker(
            update_config, 
            self.on_update_config_finished, 
            self.token, websocket_url, self.client_id
        )
    
    @Slot(object, str)
    def on_update_config_finished(self, success, error):
        """更新配置完成事件"""
        self.set_ui_enabled(True)
        self.progress_bar.setValue(0)
        self.progress_bar.setFormat("就绪")
        
        if success:
            self.status_label.setText("配置更新成功")
        else:
            self.status_label.setText("配置更新失败")
            QMessageBox.warning(self, "错误", f"配置更新失败: {error}")
    
    @Slot()
    def on_build_clicked(self):
        """编译固件按钮点击事件"""
        self.set_ui_enabled(False)
        self.status_label.setText("正在编译固件...")
        self.progress_bar.setValue(50)
        self.progress_bar.setFormat("编译固件...")
        
        # 启动 worker
        self.worker = self.start_worker(build_firmware, self.on_build_finished, self.idf_path, skip_clean=False)
        if self.worker:
            self.worker.log_batch_signal.connect(self.update_log_batch)
    
    @Slot(object, str)
    def on_build_finished(self, success, error):
        """编译固件完成事件"""
        self.set_ui_enabled(True)
        self.progress_bar.setValue(0)
        self.progress_bar.setFormat("就绪")
        
        if success:
            self.status_label.setText("固件编译成功")
        else:
            self.status_label.setText("固件编译失败")
            QMessageBox.warning(self, "错误", f"固件编译失败: {error}")
    
    @Slot()
    def on_flash_clicked(self):
        """烧录固件按钮点击事件"""
        port = self.get_selected_port()
        if not port:
            QMessageBox.warning(self, "错误", "请选择一个串口")
            return
            
        self.set_ui_enabled(False)
        self.status_label.setText("正在烧录固件...")
        self.progress_bar.setValue(70)
        self.progress_bar.setFormat("烧录固件...")
        
        # 启动 worker
        self.worker = self.start_worker(flash_firmware, self.on_flash_finished, port, self.idf_path)
        if self.worker:
            self.worker.log_batch_signal.connect(self.update_log_batch)
    
    @Slot(object, str)
    def on_flash_finished(self, success, error):
        """烧录固件完成事件"""
        self.set_ui_enabled(True)
        self.progress_bar.setValue(0)
        self.progress_bar.setFormat("就绪")
        
        if success:
            self.status_label.setText("固件烧录成功")
        else:
            self.status_label.setText("固件烧录失败")
            QMessageBox.warning(self, "错误", f"固件烧录失败: {error}")
    
    @Slot()
    def on_all_in_one_clicked(self):
        """一键烧录按钮点击事件"""
        port = self.get_selected_port()
        if not port:
            QMessageBox.warning(self, "错误", "请选择一个串口")
            return
            
        websocket_url = self.websocket_url_edit.text()
        if not websocket_url:
            QMessageBox.warning(self, "错误", "WebSocket URL不能为空")
            return
            
        self.set_ui_enabled(False)
        self.progress_bar.setValue(10)
        self.progress_bar.setFormat("开始一键烧录流程...")
        
        # 启动一键烧录流程
        self.perform_all_in_one(port, websocket_url)
    
    def perform_all_in_one(self, port, websocket_url):
        """执行一键烧录流程"""
        # 1. 获取MAC地址
        self.status_label.setText("第1步: 获取MAC地址...")
        self.progress_bar.setValue(10)
        self.progress_bar.setFormat("获取MAC地址 (10%)")
        
        # 使用特殊的回调，传递额外参数给下一步
        self.worker = self.start_worker(
            get_device_mac, 
            lambda mac, error: self.all_in_one_step2(mac, error, port, websocket_url),
            port
        )
    
    def all_in_one_step2(self, mac_address, error, port, websocket_url):
        """一键烧录第2步: 注册设备"""
        if not mac_address:
            self.all_in_one_failed("获取MAC地址失败", error)
            return
            
        # 更新MAC显示
        self.mac_address = mac_address
        self.mac_display.setText(mac_address)
        
        # 继续注册设备
        self.status_label.setText("第2步: 注册设备...")
        self.progress_bar.setValue(30)
        self.progress_bar.setFormat("注册设备 (30%)")
        
        # 这里使用专门为一键流程设计的回调，传递参数给下一步
        self.worker = self.start_worker(
            register_device,
            lambda result, error: self.all_in_one_step3(result, error, port, websocket_url),
            mac_address
        )
    
    def all_in_one_step3(self, result, error, port, websocket_url):
        """一键烧录第3步: 更新配置"""
        if not result or not isinstance(result, tuple) or len(result) < 2:
            self.all_in_one_failed("注册设备失败", error)
            return
            
        token, client_id = result[0], result[1]
        bind_key = result[2] if len(result) > 2 else None
        
        if not token:
            self.all_in_one_failed("注册设备失败: 未获取到token", error)
            return
            
        # 更新token和client_id显示
        self.token = token
        self.client_id = client_id
        self.bind_key = bind_key
        self.token_display.setText(token)
        self.client_id_display.setText(client_id if client_id else "使用MAC地址替代")
        
        # 继续更新配置
        self.status_label.setText("第3步: 更新配置...")
        self.progress_bar.setValue(50)
        self.progress_bar.setFormat("更新配置 (50%)")
        
        self.worker = self.start_worker(
            update_config, 
            lambda success, error: self.all_in_one_step4(success, error, port, websocket_url), 
            self.token, websocket_url, self.client_id
        )
    
    def all_in_one_step4(self, success, error, port, websocket_url):
        """一键烧录第4步: 编译固件"""
        if not success:
            self.all_in_one_failed("更新配置失败", error)
            return
            
        # 继续编译固件
        self.status_label.setText("第4步: 编译固件...")
        self.progress_bar.setValue(70)
        self.progress_bar.setFormat("编译固件 (70%)")
        
        self.worker = self.start_worker(build_firmware,
                                         lambda success, error: self.all_in_one_step5(success, error, port),
                                         self.idf_path, skip_clean=True)
        if self.worker:
             self.worker.log_batch_signal.connect(self.update_log_batch)
    
    def all_in_one_step5(self, success, error, port):
        """一键烧录第5步: 烧录固件"""
        if not success:
            self.all_in_one_failed("编译固件失败", error)
            return
            
        # 继续烧录固件
        self.status_label.setText("第5步: 烧录固件...")
        self.progress_bar.setValue(90)
        self.progress_bar.setFormat("烧录固件 (90%)")
        
        self.worker = self.start_worker(flash_firmware,
                                         self.all_in_one_completed,
                                         port, self.idf_path)
        if self.worker:
             self.worker.log_batch_signal.connect(self.update_log_batch)
    
    def all_in_one_completed(self, success, error):
        """一键烧录完成"""
        self.set_ui_enabled(True)
        
        if success:
            self.progress_bar.setValue(100)
            self.progress_bar.setFormat("烧录完成 (100%)")
            self.status_label.setText("一键烧录流程完成")
            
            if self.bind_key:
                QMessageBox.information(self, "成功", f"设备烧录成功!\n\n设备绑定码: {self.bind_key}")
            else:
                QMessageBox.information(self, "成功", "设备烧录成功!")
        else:
            self.all_in_one_failed("烧录固件失败", error)
    
    def all_in_one_failed(self, stage, error):
        """一键烧录失败处理"""
        self.set_ui_enabled(True)
        self.progress_bar.setValue(0)
        self.progress_bar.setFormat("失败")
        self.status_label.setText(f"一键烧录失败: {stage}")
        
        QMessageBox.critical(self, "失败", f"一键烧录失败\n\n步骤: {stage}\n错误: {error}")

    def closeEvent(self, event):
        """窗口关闭事件，确保所有线程安全终止"""
        logger.debug(f"正在关闭应用程序，终止{len(self._active_threads)}个活动线程...")
        
        # 请求所有线程停止
        for thread in self._active_threads:
            thread.stop()
        
        # 等待所有线程完成
        for i, thread in enumerate(self._active_threads):
            logger.debug(f"等待线程{i+1}/{len(self._active_threads)}完成...")
            if not thread.wait(3000):  # 等待3秒
                logger.debug(f"线程{i+1}未能及时完成，强制终止")
                thread.terminate()
                thread.wait(1000)
        
        # 清空线程列表
        self._active_threads.clear()
        
        # 移除日志处理器
        if hasattr(self, 'log_redirector') and self.log_redirector.handler:
            logger.debug("移除日志处理器...")
            logger.removeHandler(self.log_redirector.handler)
        
        event.accept()

    def start_worker(self, function, callback, *args, **kwargs):
        """安全启动工作线程"""
        # 先清理已完成的线程
        if not hasattr(self, '_active_threads'): # 确保列表已初始化
             self._active_threads = []
        self._active_threads = [t for t in self._active_threads if t.isRunning()]

        # 创建新线程
        worker = WorkerThread(function, *args, **kwargs)

        # 连接 finished 信号
        worker.finished.connect(callback)
        # 添加线程完成时的清理操作
        worker.finished.connect(lambda *_: self._remove_thread(worker))

        self._active_threads.append(worker)
        logger.debug(f"启动新工作线程({function.__name__})，当前活动线程数: {len(self._active_threads)}")
        worker.safe_start() # 使用安全启动（带超时）
        return worker
    
    def _remove_thread(self, thread):
        """从活动线程列表中移除线程"""
        if thread in self._active_threads:
            self._active_threads.remove(thread)
            logger.debug(f"线程完成，当前活动线程数: {len(self._active_threads)}")

    @Slot(str)
    def update_log_batch(self, batch_text):
        """更新日志显示 (处理批量日志)"""
        self.log_text.moveCursor(QTextCursor.End)
        self.log_text.insertPlainText(batch_text) # 使用 PlainText 提高性能
        self.log_text.moveCursor(QTextCursor.End) # 确保滚动到底部

def run_ui():
    """运行GUI界面"""
    app = QApplication(sys.argv)
    
    # 设置应用样式
    app.setStyle("Fusion")
    
    # 创建并显示主窗口
    window = ShaoluUI()
    window.show()
    
    # 添加应用退出前的处理
    app.aboutToQuit.connect(lambda: logger.debug("应用程序即将退出"))
    
    return app.exec()

# 修改主函数，支持GUI模式
def main():
    logger.info("=== 独众固件自动烧录工具 ===")
    
    # 检查是否应该使用GUI模式
    if not args.cli and HAS_PYSIDE6:
        logger.info("启动GUI模式")
        return run_ui()
    
    # 命令行模式逻辑
    try:
        # 检查ESP-IDF是否已安装
        is_installed, idf_path = check_esp_idf_installed()
        if not is_installed:
            return 1
        
        # 原有的命令行模式逻辑
        # ...此处省略原有的命令行模式代码...
        
        # 确定使用哪个串口
        port = args.port
        if not port:
            logger.info("自动检测ESP32设备串口...")
            available_ports = detect_ports()
            if not available_ports:
                logger.error("未检测到可用串口设备，请手动指定 --port 参数")
                return 1
                
            port = select_port(available_ports, args.auto_select)
            if not port:
                return 1
        
        # 1. 获取设备MAC地址
        mac_address = get_device_mac(port)
        if not mac_address:
            logger.error("无法获取设备MAC地址，退出")
            return 1
        
        # 2. 通过API入库设备，获取token和clientId
        token, client_id, bind_key = register_device(mac_address)
        if not token:
            logger.error("无法获取设备token，退出")
            return 1
        
        # 使用API返回的clientId，如果API没返回，脚本中已有降级处理
        # 3. 更新SDK配置
        if not update_config(token, args.websocket_url, client_id):
            logger.error("更新配置失败，退出")
            return 1
        
        # 4. 编译固件
        if not build_firmware(idf_path, args.skip_clean):
            logger.error("编译固件失败，退出")
            return 1
        
        # 5. 烧录固件
        if not flash_firmware(port, idf_path):
            logger.error("烧录固件失败，退出")
            return 1
        
        logger.info("===== 设备自动烧录完成 =====")
        logger.info(f"串口: {port}")
        logger.info(f"MAC地址: {mac_address}")
        logger.info(f"Token: {token}")
        logger.info(f"Websocket URL: {args.websocket_url}")
        return 0
    except KeyboardInterrupt:
        logger.info("\n用户取消操作")
        return 1
    except Exception as e:
        logger.error(f"程序执行过程中发生未捕获的异常: {e}")
        logger.debug(traceback.format_exc())
        return 1
    finally:
        cleanup()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="独众固件自动烧录工具")
    parser.add_argument("--port", "-p", help="设备串口，例如 COM3 或 /dev/ttyUSB0")
    parser.add_argument("--websocket_url", help=f"Websocket URL，默认为 {DEFAULT_WEBSOCKET_URL}", default=DEFAULT_WEBSOCKET_URL)
    parser.add_argument("--auto-select", action="store_true", help="当检测到多个串口时自动选择第一个")
    parser.add_argument("--debug", action="store_true", help="启用调试日志")
    parser.add_argument("--cli", action="store_true", help="强制使用命令行模式而非GUI")
    parser.add_argument("--skip-clean", action="store_true", help="跳过清理步骤")
    args = parser.parse_args()
    
    # 设置日志级别
    if args.debug:
        logger.setLevel(logging.DEBUG)
    
    sys.exit(main())