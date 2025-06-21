#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import json
import re
import requests
import argparse

try:
    import serial.tools.list_ports
except ImportError:
    print("请安装pyserial库: pip install pyserial")
    sys.exit(1)

# 基本配置
API_URL = "https://xiaoqiao-api.oaibro.com/admin-api/xiaoqiao/admin/device/add"  # 设备入库API地址

def check_esp_idf_installed():
    """检查ESP-IDF开发环境是否已安装"""
    print("检查ESP-IDF开发环境...")
    
    # 检查方法1: 查找环境变量
    idf_path_env = os.environ.get("IDF_PATH")
    if idf_path_env and os.path.exists(idf_path_env):
        print(f"检测到ESP-IDF环境变量: {idf_path_env}")
        return True
    
    # 检查方法2: 检查常见安装路径
    common_paths = [
        "C:\\Users\\1\\esp\\v5.3.2\\esp-idf",
        "C:\\esp\\esp-idf",
        os.path.expanduser("~/esp/esp-idf"),
        "C:\\Espressif\\frameworks\\esp-idf-v5.3.2"
    ]
    
    for path in common_paths:
        if os.path.exists(path):
            print(f"检测到ESP-IDF安装路径: {path}")
            return True
    
    # 检查方法3: 尝试运行idf.py命令
    try:
        result = subprocess.run("idf.py --version", shell=True, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"检测到ESP-IDF命令可用: {result.stdout.strip()}")
            return True
    except Exception:
        pass
    
    # 未检测到ESP-IDF
    print("\n错误: 未检测到ESP-IDF开发环境!")
    print("请按照以下步骤安装ESP-IDF环境:")
    print("1. 访问: https://dl.espressif.com/dl/esp-idf/")
    print("2. 下载并安装ESP-IDF v5.3.2或更高版本")
    print("3. 按照安装向导完成安装")
    print("4. 安装完成后重新运行此脚本\n")
    return False

def detect_ports():
    """自动检测可能的ESP32设备串口"""
    ports = []
    
    # 列出所有可用串口
    available_ports = list(serial.tools.list_ports.comports())
    
    # 根据常见ESP32设备的特征筛选串口
    for port in available_ports:
        # 常见ESP32设备的USB转串口芯片厂商ID或描述
        if any(keyword in port.description.lower() for keyword in ['cp210', 'ch340', 'ftdi', 'usb serial', 'silicon labs']):
            ports.append(port.device)
        # 如果找不到特定厂商，则添加所有串口设备
    
    if not ports and available_ports:
        # 如果没有找到匹配的端口但有其他端口，全部添加
        ports = [port.device for port in available_ports]
    
    return ports

def select_port(ports, auto_select=True):
    """选择一个串口，如果auto_select为True，则自动选择第一个"""
    if not ports:
        print("未找到可用串口设备")
        return None
    
    if len(ports) == 1 or auto_select:
        selected_port = ports[0]
        print(f"自动选择串口: {selected_port}")
        return selected_port
    
    # 让用户选择串口
    print("检测到多个可能的串口:")
    for i, port in enumerate(ports):
        print(f"{i+1}. {port}")
    
    while True:
        try:
            choice = int(input("请选择ESP32连接的串口 [1-{}]: ".format(len(ports))))
            if 1 <= choice <= len(ports):
                return ports[choice-1]
        except ValueError:
            pass
        print("无效选择，请重试")

def get_device_mac(port):
    """获取设备的MAC地址"""
    try:
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
        
        print(f"使用esptool路径: {esptool_path}")
        
        # 尝试使用Python调用esptool模块
        cmd = [sys.executable, "-m", "esptool", "--port", port, "read_mac"]
        print(f"执行命令: {' '.join(cmd)}")
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"使用Python模块运行失败: {result.stderr}")
            
            # 尝试直接调用esptool.py
            cmd = [esptool_path, "--port", port, "read_mac"]
            print(f"尝试直接调用: {' '.join(cmd)}")
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode != 0:
                print(f"获取MAC地址失败: {result.stderr}")
                print("请确保安装了esptool: pip install esptool")
                print("如果您使用ESP-IDF，请确保已执行ESP-IDF环境设置脚本")
                print("Windows: %IDF_PATH%\\export.bat")
                print("Linux/macOS: source $IDF_PATH/export.sh")
                return None
        
        # 从输出中解析MAC地址
        print(f"命令输出: {result.stdout}")
        mac_match = re.search(r'MAC:\s+([0-9a-f:]{17})', result.stdout)
        if not mac_match:
            print(f"无法从输出中解析MAC地址")
            return None
        
        mac_address = mac_match.group(1)
        print(f"获取到设备MAC地址: {mac_address}")
        return mac_address
    except Exception as e:
        print(f"获取MAC地址时发生错误: {str(e)}")
        import traceback
        traceback.print_exc()
        return None

def register_device(mac_address):
    """通过API注册设备，获取token和clientId"""
    try:
        print(f"正在通过API注册设备 (MAC: {mac_address})...")
        headers = {
            "Content-Type": "application/json"
        }
        data = {
            "clientType": "esp32",
            "deviceId": mac_address,
            "deviceName": f"ESP32设备-{mac_address.replace(':', '')[-4:]}",
            "deviceVersion": "1.0.0"
        }
        
        print(f"发送请求到: {API_URL}")
        print(f"请求数据: {json.dumps(data, ensure_ascii=False)}")
        
        response = requests.post(API_URL, headers=headers, json=data)
        print(f"API响应状态码: {response.status_code}")
        print(f"API响应内容: {response.text}")
        
        response.raise_for_status()
        
        result = response.json()
        if result.get("code") != 0:
            print(f"设备入库失败: {result.get('msg', '未知错误')}")
            return None, None
            
        token = result.get("data", {}).get("apiKey")
        client_id = result.get("data", {}).get("clientId")
        
        if not token:
            print(f"API响应中没有apiKey: {result}")
            return None, None
            
        if not client_id:
            print(f"API响应中没有clientId: {result}")
            client_id = mac_address.replace(':', '')  # 降级到默认行为
        
        print(f"设备入库成功:")
        print(f"- Token: {token}")
        print(f"- Client ID: {client_id}")
        
        return token, client_id
    except Exception as e:
        print(f"设备入库失败: {e}")
        return None, None

def update_config(token, client_id=None):
    """更新sdkconfig文件中的连接类型设置和Client-Id"""
    try:
        print("正在更新sdkconfig配置...")
        
        # 读取sdkconfig文件内容
        with open("sdkconfig", "r", encoding="utf-8") as f:
            content = f.read()
        
        # 使用正则表达式替换配置项
        import re
        
        # 确保WebSocket模式启用
        content = re.sub(r'# CONFIG_CONNECTION_TYPE_WEBSOCKET is not set',
                        'CONFIG_CONNECTION_TYPE_WEBSOCKET=y',
                        content)
        
        if 'CONFIG_CONNECTION_TYPE_MQTT_UDP=y' in content:
            content = content.replace('CONFIG_CONNECTION_TYPE_MQTT_UDP=y', 
                                    '# CONFIG_CONNECTION_TYPE_MQTT_UDP is not set')
        
        # 设置Client-Id配置
        if client_id:
            client_id_config = f'CONFIG_WEBSOCKET_CLIENT_ID="{client_id}"'
            # 查找并替换现有的CLIENT_ID配置
            if 'CONFIG_WEBSOCKET_CLIENT_ID=' in content:
                content = re.sub(r'CONFIG_WEBSOCKET_CLIENT_ID=.*', client_id_config, content)
            else:
                # 如果没有找到，在CONNECTION_TYPE_WEBSOCKET配置后添加
                content = re.sub(r'(CONFIG_CONNECTION_TYPE_WEBSOCKET=y)',
                               r'\1\n' + client_id_config,
                               content)
        
        # 写回sdkconfig文件
        with open("sdkconfig", "w", encoding="utf-8") as f:
            f.write(content)
        
        print(f"已更新sdkconfig:")
        print(f"- 连接类型: WebSocket")
        if client_id:
            print(f"- Client-Id: {client_id}")
        print(f"- WebSocket URL将从OTA接口动态获取")
        
        return True
    except Exception as e:
        print(f"更新配置时发生错误: {str(e)}")
        import traceback
        traceback.print_exc()
        return False

def build_firmware():
    """编译固件"""
    try:
        print("正在编译固件...")
        
        # 使用ESP-IDF安装路径
        idf_path = "C:\\Users\\1\\esp\\v5.4.1\\esp-idf"
        
        # 先执行fullclean清理项目
        activate_cmd = f"call {idf_path}\\export.bat && idf.py fullclean"
        print(f"执行命令: {activate_cmd}")
        
        process = subprocess.Popen(activate_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in iter(process.stdout.readline, ''):
            print(line, end='')
        process.wait()
        
        # 然后再编译
        activate_cmd = f"call {idf_path}\\export.bat && idf.py build"
        print(f"执行命令: {activate_cmd}")
        
        process = subprocess.Popen(activate_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in iter(process.stdout.readline, ''):
            print(line, end='')
        
        process.wait()
        if process.returncode != 0:
            print("编译固件失败")
            return False
        
        print("固件编译成功")
        return True
    except Exception as e:
        print(f"编译固件时发生错误: {str(e)}")
        import traceback
        traceback.print_exc()
        return False

def flash_firmware(port):
    """烧录固件"""
    try:
        print(f"正在烧录固件到设备 (端口: {port})...")
        
        # 使用您的ESP-IDF具体安装路径
        idf_path = "C:\\Users\\1\\esp\\v5.4.1\\esp-idf"
        
        # 先执行擦除操作
        erase_cmd = f"call {idf_path}\\export.bat && idf.py -p {port} erase_flash"
        print(f"执行擦除命令: {erase_cmd}")
        print("正在擦除Flash...")
        
        erase_process = subprocess.Popen(erase_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        
        for line in iter(erase_process.stdout.readline, ''):
            print(line, end='')
            
        erase_process.wait()
        if erase_process.returncode != 0:
            print("Flash擦除失败")
            return False
        
        print("Flash擦除完成，开始烧录...")
        
        # 执行export.bat激活环境并烧录
        activate_cmd = f"call {idf_path}\\export.bat && idf.py -p {port} flash"
        print(f"执行烧录命令: {activate_cmd}")
        
        process = subprocess.Popen(activate_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        
        for line in iter(process.stdout.readline, ''):
            print(line, end='')
            
        process.wait()
        if process.returncode != 0:
            print("烧录固件失败")
            return False
        
        print("固件烧录成功")
        return True
    except Exception as e:
        print(f"烧录固件时发生错误: {str(e)}")
        import traceback
        traceback.print_exc()
        return False

def main():
    print("=== ESP32S3自动烧录工具 ===")
    
    # 检查ESP-IDF是否已安装
    if not check_esp_idf_installed():
        return 1
    
    # 确定使用哪个串口
    port = args.port
    if not port:
        print("自动检测ESP32设备串口...")
        available_ports = detect_ports()
        if not available_ports:
            print("未检测到可用串口设备，请手动指定 --port 参数")
            return 1
            
        port = select_port(available_ports, args.auto_select)
        if not port:
            return 1
    
    # 1. 获取设备MAC地址
    mac_address = get_device_mac(port)
    if not mac_address:
        print("无法获取设备MAC地址，退出")
        return 1
    
    # 2. 通过API入库设备，获取token和clientId
    token, client_id = register_device(mac_address)
    if not token:
        print("无法获取设备token，退出")
        return 1
    
    # 3. 更新SDK配置，将client_id写入配置作为设备的永久标识符
    if not update_config(token, client_id):
        print("更新配置失败，退出")
        return 1
    
    # 4. 编译固件
    if not build_firmware():
        print("编译固件失败，退出")
        return 1
    
    # 5. 烧录固件
    if not flash_firmware(port):
        print("烧录固件失败，退出")
        return 1
    
    print("===== 设备自动烧录完成 =====")
    print(f"串口: {port}")
    print(f"MAC地址: {mac_address}")
    print(f"Client-Id: {client_id}")
    print(f"设备已入库，Client-Id已写入配置作为永久标识符")
    return 0

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ESP32S3自动烧录工具")
    parser.add_argument("--port", "-p", help="设备串口，例如 COM3 或 /dev/ttyUSB0")
    parser.add_argument("--auto-select", action="store_true", help="当检测到多个串口时自动选择第一个")
    args = parser.parse_args()
    
    sys.exit(main())