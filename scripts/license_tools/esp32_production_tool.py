#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
小智AI商用授权 - ESP32生产用途的固件烧录工具
terrence@tenclass.com
支持自动检测USB设备、读取MAC地址、获取授权信息、下载并烧录固件

# 烧录序列号方法：
# espefuse.py -p /dev/ttyACM0 burn_block_data BLOCK_USR_DATA serial_number
# espefuse.py -p /dev/ttyACM0 burn_key BLOCK_KEY0 license_key HMAC_UP
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import time
import os
import sys
import subprocess
from typing import Dict
import requests
import json
from urllib.parse import urlparse, parse_qs, urlencode, urlunparse
import serial.tools.list_ports
import re
from concurrent.futures import ThreadPoolExecutor
import asyncio
import threading
from functools import partial

class DeviceManager:
    """设备管理器，负责单个设备的管理和烧录"""
    def __init__(self, port_num, parent, exact_name=None):
        self.exact_name = exact_name
        self.port_num = port_num
        self.parent: "ESP32ProductionTool" = parent
        self.device_path = None
        self.mac_address = None
        self.status = "未连接"
        self.is_flashing = False
        self.frame = None
        self.status_label = None
        self.info_label = None
        self.button = None
        self.current_process = None  # 存储当前烧录进程
        
    def log(self, message):
        """记录日志，自动添加端口前缀 - 线程安全版本"""
        self.parent.log(f"端口{self.port_num}: {message}")
        
    def update_status(self, status, device_path=None):
        """更新设备状态"""
        self.status = status
        if device_path:
            self.device_path = device_path
        elif status == '未连接':
            self.device_path = None
            self.mac_address = None
            # 如果设备断开，终止当前烧录进程
            if self.current_process is not None:
                try:
                    self.current_process.terminate()
                    self.current_process.wait(timeout=1)
                except:
                    pass
                self.current_process = None
                self.is_flashing = False
            
        if self.frame:
            # 映射状态到样式名称
            style_map = {
                "未连接": "Disconnected.TFrame",
                "已连接": "Connected.TFrame",
                "完成": "Completed.TFrame",
                "正在烧录": "Working.TFrame",
                "失败": "Failed.TFrame"
            }
            style_name = style_map.get(status, "Disconnected.TFrame")
            
            # 使用after方法确保在主线程中更新UI
            def update_ui():
                self.frame.config(style=style_name)
                self.status_label.config(text=status)
                if self.device_path:
                    info_text = self.device_path
                    if self.mac_address:
                        info_text += f"\n{self.mac_address}"
                    self.info_label.config(text=info_text)
                else:
                    self.info_label.config(text="")
                    
                if status == "未连接":
                    self.button.config(state='disabled')
                elif status == "完成":
                    self.button.config(state='disabled')
                elif status == "正在烧录":
                    self.button.config(state='disabled')
                elif status == "失败":
                    self.button.config(state='normal')
                else:  # 已连接
                    self.button.config(state='normal')
                    
            self.parent.root.after(0, update_ui)
            
    def read_info(self):
        """读取设备信息 - 异步执行"""
        # 在线程池中异步执行
        future = self.parent.executor.submit(self._read_info_async)
        return future
    
    def _read_info_async(self):
        """读取设备信息 - 同步执行版本"""
        try:
            if not self.device_path:
                self.log("设备不存在")
                return None
                
            # 读取MAC地址
            if sys.platform.startswith('win'):
                cmd = ["esptool", "-c", "auto", "-p", self.device_path, "read-mac"]
            else:
                cmd = ["esptool.py", "--chip", "auto", "--port", self.device_path, "read_mac"]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode != 0:
                self.log(f"读取MAC地址失败: {result.stderr}")
                return None
                
            # 使用正则表达式提取第一个MAC地址
            print(result.stdout)
            mac_pattern = r"MAC:(\s+)([0-9a-fA-F:]+)"
            match = re.search(mac_pattern, result.stdout)
            if not match:
                self.log("无法解析MAC地址")
                return None
                
            self.mac_address = match.group(2)  # 修正：使用group(2)获取MAC地址
            self.log(f"MAC地址: {self.mac_address}")
            
            # 检查是否已烧录序列号
            if sys.platform.startswith("win"):
                cmd = ["espefuse", "--port", self.device_path, "--do-not-confirm", "summary"]
            else:
                cmd = ["espefuse.py", "--port", self.device_path, "--do-not-confirm", "summary"]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode != 0:
                self.log(f"读取序列号失败: {result.stderr}")
                return None
                
            if "HMAC_UP" in result.stdout:
                self.log("设备已烧录序列号")
                self.parent.root.after(0, lambda: self.update_status("完成"))
            else:
                self.log("设备未烧录序列号")
                self.parent.root.after(0, lambda: self.update_status("已连接"))
                
            return True
            
        except subprocess.TimeoutExpired:
            self.log("读取设备信息超时")
            return None
        except Exception as e:
            self.log(f"读取设备信息失败: {str(e)}")
            return None
            
    def flash(self):
        """烧录设备 - 异步执行"""
        def _flash_async():
            if self.is_flashing:
                return
                
            self.is_flashing = True
            try:
                # 立即更新状态为正在烧录
                self.parent.root.after(0, lambda: self.update_status("正在烧录"))
                self.log("开始烧录")
                
                # 获取授权信息
                self.log("正在获取授权信息...")
                license_data = self._get_license_info_sync()
                if not license_data:
                    self.parent.root.after(0, lambda: self.update_status("失败"))
                    self.log("获取授权信息失败")
                    return
                    
                # 下载固件
                if not license_data.get('firmware'):
                    self.log("固件信息不存在，请在后台配置最新版固件")
                    self.parent.root.after(0, lambda: self.update_status("失败"))
                    return
                
                self.log("正在下载固件...")
                firmware_path = self._download_firmware_sync(license_data['firmware']['image_url'])
                if not firmware_path:
                    self.parent.root.after(0, lambda: self.update_status("失败"))
                    self.log("下载固件失败")
                    return
                    
                # 烧录设备
                try:
                    # 烧录固件
                    self.log("开始烧录固件...")
                    if not self._flash_firmware_sync(firmware_path):
                        self.parent.root.after(0, lambda: self.update_status("失败"))
                        self.log("烧录固件失败")
                        return
                        
                    # 烧录序列号和授权密钥
                    self.log("开始烧录授权信息...")
                    if not self._flash_license_info_sync(license_data):
                        self.parent.root.after(0, lambda: self.update_status("失败"))
                        self.log("烧录授权信息失败")
                        return
                        
                    self.parent.root.after(0, lambda: self.update_status("完成"))
                    self.log("烧录完成")
                    
                except Exception as e:
                    self.log(f"烧录失败: {str(e)}")
                    self.parent.root.after(0, lambda: self.update_status("失败"))
                    
            finally:
                self.is_flashing = False
        
        # 在线程池中异步执行
        future = self.parent.executor.submit(_flash_async)
        return future

    def get_license_info(self):
        """获取授权信息 - 异步版本"""
        def _get_license_info_async():
            return self._get_license_info_sync()
        
        future = self.parent.executor.submit(_get_license_info_async)
        return future
    
    def _get_license_info_sync(self):
        """获取授权信息 - 同步版本"""
        try:
            if not self.mac_address:
                self.log("无法获取MAC地址")
                return None
                
            # 从配置文件读取授权URL
            try:
                with open(self.parent.config_file, 'r', encoding='utf-8') as f:
                    config = json.load(f)
                    license_url = config.get('license_url', '')
            except Exception as e:
                self.log(f"读取配置文件失败: {str(e)}")
                return None
                
            if not license_url:
                self.log("请在配置文件中设置授权链接")
                return None
                
            # 解析URL并替换seed参数
            parsed_url = urlparse(license_url)
            query_params = parse_qs(parsed_url.query)
            
            # 替换seed参数
            query_params['seed'] = [self.mac_address]
            
            # 重新构建URL
            new_query = urlencode(query_params, doseq=True)
            new_url = urlunparse((
                parsed_url.scheme,
                parsed_url.netloc,
                parsed_url.path,
                parsed_url.params,
                new_query,
                parsed_url.fragment
            ))
            
            # 发送请求
            self.log(f"正在请求授权信息: {new_url[:80]}...")
            response = requests.get(new_url, timeout=30)
            
            if response.status_code != 200:
                self.log(f"获取授权信息失败: HTTP {response.status_code}")
                return None
                
            try:
                data = response.json()
                if data.get('success') != True:
                    self.log(f"获取授权信息失败: {data.get('message', '未知错误')}")
                    return None
                    
                return data.get('data')
            except Exception as e:
                self.log(f"解析授权信息失败: {str(e)}")
                return None
                
        except Exception as e:
            self.log(f"获取授权信息失败: {str(e)}")
            return None

    def download_firmware(self, firmware_url):
        """下载固件 - 异步版本"""
        def _download_firmware_async():
            return self._download_firmware_sync(firmware_url)
        
        future = self.parent.executor.submit(_download_firmware_async)
        return future
    
    def _download_firmware_sync(self, firmware_url):
        """下载固件 - 同步版本"""
        try:
            # 创建固件缓存目录
            cache_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "firmware_cache")
            if not os.path.exists(cache_dir):
                os.makedirs(cache_dir)
                self.log("创建固件缓存目录: firmware_cache")
            
            # 从URL中提取文件名
            filename = os.path.basename(firmware_url)
            firmware_path = os.path.join(cache_dir, filename)
            
            # 如果文件已存在，直接返回路径
            if os.path.exists(firmware_path):
                self.log(f"使用已下载的固件: {filename}")
                return firmware_path
                
            # 下载固件
            self.log(f"正在下载固件: {firmware_url}")
            response = requests.get(firmware_url, stream=True, timeout=60)
            response.raise_for_status()
            
            # 保存固件
            with open(firmware_path, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    if chunk:
                        f.write(chunk)
                        
            self.log(f"固件下载完成: firmware_cache/{filename}")
            return firmware_path
            
        except Exception as e:
            self.log(f"下载固件失败: {str(e)}")
            return None
            
    def flash_firmware(self, firmware_path):
        """烧录固件 - 异步版本"""
        def _flash_firmware_async():
            return self._flash_firmware_sync(firmware_path)
        
        future = self.parent.executor.submit(_flash_firmware_async)
        return future
    
    def _flash_firmware_sync(self, firmware_path):
        """烧录固件 - 同步版本"""
        try:
            if not os.path.exists(firmware_path):
                self.log(f"固件文件不存在: {firmware_path}")
                return False
                
            cmd = [
                "esptool.py",
                "--chip", "auto",
                "--port", self.device_path,
                "--baud", "921600",
                "--before", "default_reset",
                "--after", "hard_reset",
                "write_flash",
                "0x0", firmware_path
            ]
            if sys.platform.startswith("win"):
                cmd = [
                    "esptool",
                    "--chip", "auto",
                    "--port", self.device_path,
                    "--baud", "921600",
                    "--before", "default_reset",
                    "--after", "hard_reset",
                    "write-flash",
                    "0x0", firmware_path
                ]
            
            self.log(f"执行烧录命令: {' '.join(cmd)}")
            
            self.current_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                universal_newlines=True
            )
            
            while True:
                output = self.current_process.stdout.readline()
                if output == '' and self.current_process.poll() is not None:
                    break
                if output:
                    self.log(output.strip())
                    
            return_code = self.current_process.poll()
            self.current_process = None
            
            if return_code != 0:
                self.log(f"烧录固件失败，退出码: {return_code}")
                return False
                
            self.log("固件烧录成功")
            return True
            
        except Exception as e:
            self.log(f"烧录固件时出错: {str(e)}")
            return False
            
    def flash_license_info(self, license_data):
        """烧录授权信息 - 异步版本"""
        def _flash_license_info_async():
            return self._flash_license_info_sync(license_data)
        
        future = self.parent.executor.submit(_flash_license_info_async)
        return future
    
    def _flash_license_info_sync(self, license_data):
        """烧录授权信息 - 同步版本"""
        try:
            # 创建临时文件存储序列号和授权密钥
            serial_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), f"serial_{self.port_num}.bin")
            key_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), f"key_{self.port_num}.bin")
            
            # 写入序列号
            with open(serial_file, "wb") as f:
                f.write(license_data['serial_number'].encode())
                
            # 写入授权密钥
            with open(key_file, "wb") as f:
                f.write(license_data['license_key'].encode())
                
            # 烧录序列号
            cmd = [
                "espefuse.py",
                "--port", self.device_path,
                "--do-not-confirm",
                "burn_block_data",
                "BLOCK_USR_DATA",
                serial_file
            ]
            if sys.platform.startswith("win"):
                cmd = [
                    "espefuse",
                    "--port", self.device_path,
                    "--do-not-confirm",
                    "burn-block-data",
                    "BLOCK_USR_DATA",
                    serial_file
                ]

            
            self.log("正在烧录序列号...")
            self.log(f"执行命令: {' '.join(cmd)}")
            
            self.current_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                universal_newlines=True
            )
            
            while True:
                output = self.current_process.stdout.readline()
                if output == '' and self.current_process.poll() is not None:
                    break
                if output:
                    self.log(output.strip())
                    
            return_code = self.current_process.poll()
            self.current_process = None
            
            if return_code != 0:
                self.log(f"烧录序列号失败，退出码: {return_code}")
                return False
                
            # 烧录授权密钥
            cmd = [
                "espefuse.py",
                "--port", self.device_path,
                "--do-not-confirm",
                "burn_key",
                "BLOCK_KEY0",
                key_file,
                "HMAC_UP"
            ]
            if sys.platform.startswith("win"):
                cmd = [
                    "espefuse",
                    "--port", self.device_path,
                    "--do-not-confirm",
                    "burn-key",
                    "BLOCK_KEY0",
                    key_file,
                    "HMAC_UP"
                ]

            
            self.log("正在烧录授权密钥...")
            self.log(f"执行命令: {' '.join(cmd)}")
            
            self.current_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                universal_newlines=True
            )
            
            while True:
                output = self.current_process.stdout.readline()
                if output == '' and self.current_process.poll() is not None:
                    break
                if output:
                    self.log(output.strip())
                    
            return_code = self.current_process.poll()
            self.current_process = None
            
            if return_code != 0:
                self.log(f"烧录授权密钥失败，退出码: {return_code}")
                return False
                
            # 清理临时文件
            try:
                os.remove(serial_file)
                os.remove(key_file)
            except:
                pass
                
            self.log("授权信息烧录成功")
            return True
            
        except Exception as e:
            self.log(f"烧录授权信息时出错: {str(e)}")
            return False

class ESP32ProductionTool:
    def __init__(self, root):
        self.root = root
        self.root.title("小智AI商用授权 - ESP32生产工具")
        self.root.geometry("1600x900")
        
        # 创建变量
        self.license_url = tk.StringVar()
        self.auto_refresh_enabled = True  # 自动刷新标志
        self.auto_refresh_job = None  # 存储自动刷新定时器
        self.auto_flash_enabled = True  # 自动烧录标志，默认启用
        
        # 网格布局配置，默认4x4
        self.grid_rows = 4
        self.grid_cols = 4
        
        # 全屏配置，默认启用
        self.fullscreen_on_startup = True
        
        # 设备管理器
        self.devices:Dict[str, DeviceManager] = {}  # 改为以串口名为key
        
        # 有序串口列表
        self.ordered_ports = []  # 存储排序后的串口名列表
        
        # 线程池
        self.executor = ThreadPoolExecutor(max_workers=16)  # 最多16个并发任务
        
        # 配置文件路径
        self.config_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
        
        # 加载配置
        if not self.load_config():
            # 配置失败，不继续初始化
            return
        
        # 设置窗体全屏（如果配置启用）
        if self.fullscreen_on_startup:
            self.set_fullscreen()
        
        # 创建样式
        self.create_styles()
        
        # 创建界面
        self.create_widgets()
        
        # 启动自动刷新
        self.start_auto_refresh()
        
        # 绑定快捷键
        self.root.bind('<F11>', self.toggle_fullscreen)
        self.root.bind('<Escape>', self.exit_fullscreen)
        self.root.focus_set()  # 确保窗口可以接收键盘事件
        
        # 绑定窗口关闭事件
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        
    def on_closing(self):
        """程序关闭时的清理操作"""
        self.cleanup()
        self.root.destroy()
        
    def cleanup(self):
        """清理资源"""
        try:
            # 停止自动刷新
            self.stop_auto_refresh()
            
            # 终止所有正在进行的设备操作
            for device in self.devices.values():
                if device.current_process:
                    try:
                        device.current_process.terminate()
                        device.current_process.wait(timeout=1)
                    except:
                        pass
                    device.current_process = None
                    device.is_flashing = False
            
            # 关闭线程池
            if hasattr(self, 'executor'):
                self.executor.shutdown(wait=False)
                
        except Exception as e:
            print(f"清理资源时出错: {e}")
        
    def create_styles(self):
        """创建自定义样式"""
        style = ttk.Style()
        
        # 创建端口框架样式
        style.configure('Disconnected.TFrame', background='#F5F5F5')  # 未连接
        style.configure('Connected.TFrame', background='#ADD8E6')  # 已连接
        style.configure('Completed.TFrame', background='#3CB371')  # 完成
        style.configure('Working.TFrame', background='#FF8C00')  # 正在烧录
        style.configure('Failed.TFrame', background='#DC143C')   # 失败
        
        # 创建标签样式
        style.configure('Port.TLabel', padding=2, background='#F5F5F5')
        style.configure('Status.TLabel', padding=2, background='#F5F5F5')
        style.configure('Info.TLabel', padding=2, background='#F5F5F5')
        
        # 创建按钮样式
        style.configure('Port.TButton', padding=2)
        
    def load_config(self):
        """加载配置文件"""
        try:
            if os.path.exists(self.config_file):
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    config = json.load(f)
                    license_url = config.get('license_url', '')
                    
                    # 检查是否需要进入配置模式
                    if not license_url:
                        if not self.show_config_dialog():
                            # 用户取消配置，退出程序
                            self.root.destroy()
                            return False
                    else:
                        self.license_url.set(license_url)
                        # 加载网格布局配置
                        self.grid_rows = config.get('grid_rows', 4)
                        self.grid_cols = config.get('grid_cols', 4)
                        # 加载全屏配置
                        self.fullscreen_on_startup = config.get('fullscreen_on_startup', True)
            else:
                # 配置文件不存在，进入配置模式
                if not self.show_config_dialog():
                    # 用户取消配置，退出程序
                    self.root.destroy()
                    return False
        except Exception as e:
            print(f"加载配置文件失败: {e}")
            # 配置文件损坏，进入配置模式
            if not self.show_config_dialog():
                self.root.destroy()
                return False
        return True
            
    def save_config(self):
        """保存配置文件"""
        try:
            config = {
                'grid_rows': self.grid_rows,
                'grid_cols': self.grid_cols,
                'fullscreen_on_startup': self.fullscreen_on_startup
            }
            # 如果配置文件已存在，保留原有的license_url
            if os.path.exists(self.config_file):
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    existing_config = json.load(f)
                    if 'license_url' in existing_config:
                        config['license_url'] = existing_config['license_url']
            
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(config, f, ensure_ascii=False, indent=2)
        except Exception as e:
            print(f"保存配置文件失败: {str(e)}")
            
    def set_fullscreen(self):
        """设置窗体全屏"""
        try:
            # Linux下的全屏方式
            self.root.state('zoomed')
        except tk.TclError:
            try:
                # 备用方式1
                self.root.attributes('-zoomed', True)
            except tk.TclError:
                try:
                    # 备用方式2 - 真正的全屏
                    self.root.attributes('-fullscreen', True)
                except tk.TclError:
                    # 如果都不支持，就最大化窗口
                    self.root.geometry(f"{self.root.winfo_screenwidth()}x{self.root.winfo_screenheight()}+0+0")

    def create_widgets(self):
        """创建GUI界面"""
        # 主框架
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 配置授权链接
        config_frame = ttk.LabelFrame(main_frame, text="配置", padding="10")
        config_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # 操作按钮框架
        button_frame = ttk.Frame(config_frame)
        button_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(5, 0))
        
        # 自动烧录按钮
        self.auto_flash_var = tk.BooleanVar(value=True)
        auto_flash_check = ttk.Checkbutton(
            button_frame, 
            text="自动烧录", 
            variable=self.auto_flash_var,
            command=self.toggle_auto_flash
        )
        auto_flash_check.grid(row=0, column=0, sticky=tk.W)
        
        # 端口状态网格
        ports_frame = ttk.LabelFrame(main_frame, text="端口状态", padding="10")
        ports_frame.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(0, 10))
        
        # 设置网格列的权重
        for i in range(self.grid_cols):
            ports_frame.columnconfigure(i, weight=1)
        for i in range(self.grid_rows):
            ports_frame.rowconfigure(i, weight=1)
        
        # 获取实际能查到的接口并排序
        ports_name = []
        for port in serial.tools.list_ports.comports():
            ports_name.append(port.device)
        ports_name = sorted(ports_name)
        
        # 更新有序串口列表
        self.ordered_ports = ports_name.copy()
        
        # 动态计算网格布局
        total_ports = len(ports_name)
        if total_ports == 0:
            # 没有检测到串口，创建一个占位框
            self.grid_rows = 1
            self.grid_cols = 1
        else:
            # 根据检测到的串口数量调整网格布局
            if total_ports <= 4:
                self.grid_rows = 1
                self.grid_cols = total_ports
            elif total_ports <= 8:
                self.grid_rows = 2
                self.grid_cols = 4
            elif total_ports <= 16:
                self.grid_rows = 4
                self.grid_cols = 4
            else:
                # 超过16个串口，使用更大的网格
                self.grid_cols = 8
                self.grid_rows = (total_ports + self.grid_cols - 1) // self.grid_cols
        
        # 重新设置网格列的权重
        for i in range(self.grid_cols):
            ports_frame.columnconfigure(i, weight=1)
        for i in range(self.grid_rows):
            ports_frame.rowconfigure(i, weight=1)

        # 创建网格 - 只为检测到的串口创建设备
        for idx, port_name in enumerate(ports_name):
            i = idx // self.grid_cols
            j = idx % self.grid_cols
            
            device = DeviceManager(idx, self, exact_name=port_name)
            self.devices[port_name] = device  # 使用串口名作为key
            
            # 创建固定大小的端口框架
            port_frame = ttk.Frame(ports_frame, padding="5", relief="solid", borderwidth=1, style='Disconnected.TFrame', width=256, height=144)
            port_frame.grid(row=i, column=j, padx=5, pady=5, sticky=(tk.W, tk.E, tk.N, tk.S))
            port_frame.grid_propagate(False)  # 禁止框架自动调整大小
            
            # 端口标签 - 显示实际的串口名
            port_label = ttk.Label(port_frame, text=f"{port_name}", style='Port.TLabel')
            port_label.grid(row=0, column=0, sticky=(tk.W, tk.E))
            
            # 状态标签
            status_label = ttk.Label(port_frame, text="未连接", style='Status.TLabel')
            status_label.grid(row=1, column=0, sticky=(tk.W, tk.E))
            
            # 设备信息标签
            info_label = ttk.Label(port_frame, text="", style='Info.TLabel')
            info_label.grid(row=2, column=0, sticky=(tk.W, tk.E))
            
            # 操作按钮框架
            button_frame = ttk.Frame(port_frame, style='Disconnected.TFrame')
            button_frame.grid(row=3, column=0, sticky=(tk.W, tk.E))
            
            # 烧录按钮
            flash_btn = ttk.Button(button_frame, text="烧录", style='Port.TButton',
                                 command=lambda p=port_name: self.flash_device(p))
            flash_btn.grid(row=0, column=0, sticky=(tk.W, tk.E))
            
            # 存储框架引用
            device.frame = port_frame
            device.status_label = status_label
            device.info_label = info_label
            device.button = flash_btn
            
            # 初始化状态
            device.update_status("未连接")
        
        # 如果没有检测到串口，显示提示信息
        if total_ports == 0:
            placeholder_frame = ttk.Frame(ports_frame, padding="5", relief="solid", borderwidth=1, style='Disconnected.TFrame', width=256, height=144)
            placeholder_frame.grid(row=0, column=0, padx=5, pady=5, sticky=(tk.W, tk.E, tk.N, tk.S))
            placeholder_frame.grid_propagate(False)
            
            placeholder_label = ttk.Label(placeholder_frame, text="未检测到串口设备", style='Port.TLabel')
            placeholder_label.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 日志区域
        log_frame = ttk.LabelFrame(main_frame, text="日志", padding="10")
        log_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(10, 0))
        
        self.status_text = scrolledtext.ScrolledText(log_frame, height=10)
        self.status_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 配置网格权重
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(1, weight=1)
        main_frame.rowconfigure(2, weight=1)
        config_frame.columnconfigure(0, weight=1)
        # 动态配置端口网格权重
        for i in range(self.grid_cols):
            ports_frame.columnconfigure(i, weight=1)
        for i in range(self.grid_rows):
            ports_frame.rowconfigure(i, weight=1)
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)
        
    def log(self, message):
        """记录日志 - 线程安全版本"""
        def _log_in_main_thread():
            timestamp = time.strftime("%H:%M:%S")
            log_message = f"[{timestamp}] {message}"
            self.status_text.insert(tk.END, log_message + "\n")
            self.status_text.see(tk.END)
            self.root.update_idletasks()
        
        # 如果在主线程中，直接执行；否则调度到主线程
        try:
            if threading.current_thread() == threading.main_thread():
                _log_in_main_thread()
            else:
                self.root.after(0, _log_in_main_thread)
        except:
            # 备用方案，总是调度到主线程
            self.root.after(0, _log_in_main_thread)
        
    def flash_device(self, port_name):
        """烧录指定端口的设备"""
        device = self.devices.get(port_name)
        if not device:
            messagebox.showwarning("警告", f"未找到端口 {port_name} 的设备")
            return
            
        if device.device_path and device.status in ['已连接', '失败']:  # 允许在失败状态下重新烧录
            if device.status == '失败':
                # 重置失败状态
                device.update_status('已连接')
            self.executor.submit(device.flash)
        else:
            messagebox.showwarning("警告", "请先连接设备")

    def refresh_devices(self):
        """刷新设备状态 - 异步执行，支持动态重建界面"""
        def _refresh_devices_async():
            try:
                # 获取所有USB设备
                ports = list(serial.tools.list_ports.comports())
                current_ports = sorted([port.device for port in ports])
                
                # 检查是否需要重建界面
                ports_changed = current_ports != self.ordered_ports
                
                if ports_changed:
                    # 端口列表发生变化，需要重建界面
                    self.root.after(0, lambda: self._rebuild_interface(current_ports))
                else:
                    # 端口列表没有变化，只更新状态
                    self._update_existing_devices(current_ports)
                        
            except Exception as e:
                self.root.after(0, lambda: self.log(f"刷新设备状态失败: {str(e)}"))
        
        # 在线程池中异步执行
        self.executor.submit(_refresh_devices_async)
    
    def _rebuild_interface(self, current_ports):
        """重建设备界面 - 增量更新，只处理变动的设备"""
        try:
            old_ports = set(self.ordered_ports)
            new_ports = set(current_ports)
            
            # 计算端口变化
            removed_ports = old_ports - new_ports  # 被移除的端口
            added_ports = new_ports - old_ports    # 新增的端口
            unchanged_ports = old_ports & new_ports # 未变化的端口
            
            self.log(f"端口变化: 移除{len(removed_ports)}个, 新增{len(added_ports)}个, 保持{len(unchanged_ports)}个")
            
            # 保存未变化设备的状态信息
            preserved_devices = {}
            for port_name in unchanged_ports:
                device = self.devices.get(port_name)
                if device:
                    preserved_devices[port_name] = {
                        'device': device,
                        'status': device.status,
                        'mac_address': device.mac_address,
                        'is_flashing': device.is_flashing,
                        'current_process': device.current_process
                    }
            
            # 停止并清理被移除端口的设备操作
            for port_name in removed_ports:
                device = self.devices.get(port_name)
                if device:
                    if device.current_process:
                        try:
                            device.current_process.terminate()
                            device.current_process.wait(timeout=1)
                        except:
                            pass
                        device.current_process = None
                        device.is_flashing = False
                    self.log(f"清理端口 {port_name}")
            
            # 清理所有旧的UI框架（但保留设备对象数据）
            for device in self.devices.values():
                if device.frame:
                    device.frame.destroy()
                    device.frame = None
                    device.status_label = None
                    device.info_label = None
                    device.button = None
            
            # 清空设备字典中被移除的端口
            for port_name in removed_ports:
                if port_name in self.devices:
                    del self.devices[port_name]
            
            # 更新端口列表
            self.ordered_ports = current_ports.copy()
            
            # 重新创建设备界面
            self._create_device_grid_incremental(current_ports, preserved_devices, added_ports)
            
        except Exception as e:
            self.log(f"重建界面失败: {str(e)}")
    
    def _create_device_grid_incremental(self, ports_name, preserved_devices, added_ports):
        """增量创建设备网格，保持已有设备状态"""
        # 获取ports_frame
        for widget in self.root.winfo_children():
            if isinstance(widget, ttk.Frame):
                for child in widget.winfo_children():
                    if isinstance(child, ttk.LabelFrame) and child.cget('text') == '端口状态':
                        ports_frame = child
                        break
                else:
                    continue
                break
        else:
            self.log("未找到端口状态框架")
            return
        
        # 动态计算网格布局
        total_ports = len(ports_name)
        if total_ports == 0:
            self.grid_rows = 1
            self.grid_cols = 1
        else:
            if total_ports <= 4:
                self.grid_rows = 1
                self.grid_cols = total_ports
            elif total_ports <= 8:
                self.grid_rows = 2
                self.grid_cols = 4
            elif total_ports <= 16:
                self.grid_rows = 4
                self.grid_cols = 4
            else:
                self.grid_cols = 8
                self.grid_rows = (total_ports + self.grid_cols - 1) // self.grid_cols
        
        # 重新设置网格列的权重
        for i in range(self.grid_cols):
            ports_frame.columnconfigure(i, weight=1)
        for i in range(self.grid_rows):
            ports_frame.rowconfigure(i, weight=1)

        # 创建设备网格
        for idx, port_name in enumerate(ports_name):
            i = idx // self.grid_cols
            j = idx % self.grid_cols
            
            # 检查是否是保留的设备
            if port_name in preserved_devices:
                # 恢复已有设备
                device_data = preserved_devices[port_name]
                device = device_data['device']
                device.port_num = idx  # 更新端口编号
                self.devices[port_name] = device
                
                self.log(f"恢复端口 {port_name} 设备状态: {device_data['status']}")
            else:
                # 创建新设备（包括新增的端口）
                device = DeviceManager(idx, self, exact_name=port_name)
                self.devices[port_name] = device
                self.log(f"创建新端口 {port_name} 设备")
            
            # 创建UI框架
            port_frame = ttk.Frame(ports_frame, padding="5", relief="solid", borderwidth=1, style='Disconnected.TFrame', width=256, height=144)
            port_frame.grid(row=i, column=j, padx=5, pady=5, sticky=(tk.W, tk.E, tk.N, tk.S))
            port_frame.grid_propagate(False)
            
            # 端口标签
            port_label = ttk.Label(port_frame, text=f"{port_name}", style='Port.TLabel')
            port_label.grid(row=0, column=0, sticky=(tk.W, tk.E))
            
            # 状态标签
            status_label = ttk.Label(port_frame, text="未连接", style='Status.TLabel')
            status_label.grid(row=1, column=0, sticky=(tk.W, tk.E))
            
            # 设备信息标签
            info_label = ttk.Label(port_frame, text="", style='Info.TLabel')
            info_label.grid(row=2, column=0, sticky=(tk.W, tk.E))
            
            # 操作按钮框架
            button_frame = ttk.Frame(port_frame, style='Disconnected.TFrame')
            button_frame.grid(row=3, column=0, sticky=(tk.W, tk.E))
            
            # 烧录按钮
            flash_btn = ttk.Button(button_frame, text="烧录", style='Port.TButton',
                                 command=lambda p=port_name: self.flash_device(p))
            flash_btn.grid(row=0, column=0, sticky=(tk.W, tk.E))
            
            # 存储框架引用
            device.frame = port_frame
            device.status_label = status_label
            device.info_label = info_label
            device.button = flash_btn
            
            # 恢复或初始化设备状态
            if port_name in preserved_devices:
                # 恢复保留设备的状态
                device_data = preserved_devices[port_name]
                device.status = device_data['status']
                device.mac_address = device_data['mac_address']
                device.is_flashing = device_data['is_flashing']
                device.current_process = device_data['current_process']
                
                # 恢复UI状态
                device.update_status(device.status, port_name)
            else:
                # 新设备，初始化为未连接状态
                device.update_status("未连接")
                
                # 只对新增的端口进行连接检查
                if port_name in added_ports:
                    self.executor.submit(self._check_device_connection, port_name)
        
        # 如果没有检测到串口，显示提示信息
        if total_ports == 0:
            placeholder_frame = ttk.Frame(ports_frame, padding="5", relief="solid", borderwidth=1, style='Disconnected.TFrame', width=256, height=144)
            placeholder_frame.grid(row=0, column=0, padx=5, pady=5, sticky=(tk.W, tk.E, tk.N, tk.S))
            placeholder_frame.grid_propagate(False)
            
            placeholder_label = ttk.Label(placeholder_frame, text="未检测到串口设备", style='Port.TLabel')
            placeholder_label.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
    
    def _create_device_grid(self, ports_name):
        """创建设备网格 - 用于初始创建"""
        # 获取ports_frame
        for widget in self.root.winfo_children():
            if isinstance(widget, ttk.Frame):
                for child in widget.winfo_children():
                    if isinstance(child, ttk.LabelFrame) and child.cget('text') == '端口状态':
                        ports_frame = child
                        break
                else:
                    continue
                break
        else:
            self.log("未找到端口状态框架")
            return
        
        # 动态计算网格布局
        total_ports = len(ports_name)
        if total_ports == 0:
            self.grid_rows = 1
            self.grid_cols = 1
        else:
            if total_ports <= 4:
                self.grid_rows = 1
                self.grid_cols = total_ports
            elif total_ports <= 8:
                self.grid_rows = 2
                self.grid_cols = 4
            elif total_ports <= 16:
                self.grid_rows = 4
                self.grid_cols = 4
            else:
                self.grid_cols = 8
                self.grid_rows = (total_ports + self.grid_cols - 1) // self.grid_cols
        
        # 重新设置网格列的权重
        for i in range(self.grid_cols):
            ports_frame.columnconfigure(i, weight=1)
        for i in range(self.grid_rows):
            ports_frame.rowconfigure(i, weight=1)

        # 创建设备网格
        for idx, port_name in enumerate(ports_name):
            i = idx // self.grid_cols
            j = idx % self.grid_cols
            
            device = DeviceManager(idx, self, exact_name=port_name)
            self.devices[port_name] = device
            
            # 创建固定大小的端口框架
            port_frame = ttk.Frame(ports_frame, padding="5", relief="solid", borderwidth=1, style='Disconnected.TFrame', width=256, height=144)
            port_frame.grid(row=i, column=j, padx=5, pady=5, sticky=(tk.W, tk.E, tk.N, tk.S))
            port_frame.grid_propagate(False)
            
            # 端口标签
            port_label = ttk.Label(port_frame, text=f"{port_name}", style='Port.TLabel')
            port_label.grid(row=0, column=0, sticky=(tk.W, tk.E))
            
            # 状态标签
            status_label = ttk.Label(port_frame, text="未连接", style='Status.TLabel')
            status_label.grid(row=1, column=0, sticky=(tk.W, tk.E))
            
            # 设备信息标签
            info_label = ttk.Label(port_frame, text="", style='Info.TLabel')
            info_label.grid(row=2, column=0, sticky=(tk.W, tk.E))
            
            # 操作按钮框架
            button_frame_inner = ttk.Frame(port_frame, style='Disconnected.TFrame')
            button_frame_inner.grid(row=3, column=0, sticky=(tk.W, tk.E))
            
            # 烧录按钮
            flash_btn = ttk.Button(button_frame_inner, text="烧录", style='Port.TButton',
                                 command=lambda p=port_name: self.flash_device(p))
            flash_btn.grid(row=0, column=0, sticky=(tk.W, tk.E))
            
            # 存储框架引用
            device.frame = port_frame
            device.status_label = status_label
            device.info_label = info_label
            device.button = flash_btn
            
            # 初始化状态为未连接，然后在线程池中异步检查连接状态
            device.update_status("未连接")
            
            # 异步检查设备连接状态和读取信息
            self.executor.submit(self._check_device_connection, port_name)
        
        # 如果没有检测到串口，显示提示信息
        if total_ports == 0:
            placeholder_frame = ttk.Frame(ports_frame, padding="5", relief="solid", borderwidth=1, style='Disconnected.TFrame', width=256, height=144)
            placeholder_frame.grid(row=0, column=0, padx=5, pady=5, sticky=(tk.W, tk.E, tk.N, tk.S))
            placeholder_frame.grid_propagate(False)
            
            placeholder_label = ttk.Label(placeholder_frame, text="未检测到串口设备", style='Port.TLabel')
            placeholder_label.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
    
    def _update_existing_devices(self, current_ports):
        """更新现有设备状态"""
        # 检查每个设备的连接状态
        for port_name in self.ordered_ports:
            device = self.devices.get(port_name)
            if not device:
                continue
                
            if port_name in current_ports:
                # 设备仍然存在
                if device.status == "未连接":
                    # 设备重新连接，在线程池中异步检查
                    self.executor.submit(self._check_device_connection, port_name)
            else:
                # 设备已断开
                if device.status != "未连接":
                    self.root.after(0, lambda d=device: d.update_status("未连接"))
                    self.root.after(0, lambda p=port_name: self.log(f"端口 {p} 设备已断开"))
    
    def _check_device_connection(self, port_name):
        """检查设备连接状态并读取信息 - 在线程池中执行"""
        try:
            device = self.devices.get(port_name)
            if not device:
                return
            
            # 更新设备路径
            device.device_path = port_name
            
            # 先更新状态为已连接
            self.root.after(0, lambda: device.update_status("已连接", port_name))
            self.root.after(0, lambda: self.log(f"端口 {port_name} 设备已连接"))
            
            # 在线程池中读取设备信息
            def _read_info_with_auto_flash():
                # 调用设备的read_info方法的同步版本
                device._read_info_async()
                
                # 检查是否需要自动烧录
                if device.status == "已连接" and self.auto_flash_enabled:
                    self.root.after(2000, lambda: self._check_auto_flash(port_name))
            
            # 提交读取信息任务
            self.executor.submit(_read_info_with_auto_flash)
            
        except Exception as e:
            self.root.after(0, lambda: self.log(f"检查设备连接失败 {port_name}: {str(e)}"))
    
    def _check_auto_flash(self, port_name):
        """检查是否需要自动烧录"""
        device = self.devices.get(port_name)
        if device and device.status == "已连接" and self.auto_flash_enabled:
            self.flash_device(port_name)
            
    def toggle_auto_flash(self):
        """切换自动烧录状态"""
        self.auto_flash_enabled = self.auto_flash_var.get()

    def start_auto_refresh(self):
        """开始自动刷新"""
        self.stop_auto_refresh()  # 确保之前的定时器被停止
        self.auto_refresh_enabled = True
        self.auto_refresh_job = self.root.after(1000, self.auto_refresh_devices)

    def stop_auto_refresh(self):
        """停止自动刷新"""
        self.auto_refresh_enabled = False
        if self.auto_refresh_job:
            self.root.after_cancel(self.auto_refresh_job)
            self.auto_refresh_job = None

    def auto_refresh_devices(self):
        """自动刷新设备状态"""
        if self.auto_refresh_enabled:
            self.refresh_devices()
            self.auto_refresh_job = self.root.after(1000, self.auto_refresh_devices)

    def toggle_fullscreen(self, event):
        """切换全屏状态"""
        self.root.attributes('-fullscreen', not self.root.attributes('-fullscreen'))

    def exit_fullscreen(self, event):
        """退出全屏"""
        self.root.attributes('-fullscreen', False)

    def show_config_dialog(self):
        """显示配置对话框"""
        dialog = tk.Toplevel(self.root)
        dialog.title("初始配置")
        dialog.geometry("600x400")
        dialog.resizable(False, False)
        dialog.transient(self.root)
        dialog.grab_set()
        
        # 居中显示
        dialog.update_idletasks()
        x = (dialog.winfo_screenwidth() // 2) - (600 // 2)
        y = (dialog.winfo_screenheight() // 2) - (400 // 2)
        dialog.geometry(f"600x400+{x}+{y}")
        
        # 配置变量
        license_url_var = tk.StringVar()
        grid_rows_var = tk.StringVar(value="4")
        grid_cols_var = tk.StringVar(value="4")
        fullscreen_var = tk.BooleanVar(value=True)
        
        # 主框架
        main_frame = ttk.Frame(dialog, padding="20")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # 标题
        title_label = ttk.Label(main_frame, text="ESP32生产工具 - 初始配置", font=("Arial", 14, "bold"))
        title_label.pack(pady=(0, 20))
        
        # 授权链接配置
        ttk.Label(main_frame, text="授权链接:", font=("Arial", 10, "bold")).pack(anchor=tk.W, pady=(0, 5))
        license_entry = ttk.Entry(main_frame, textvariable=license_url_var, width=70)
        license_entry.pack(fill=tk.X, pady=(0, 10))
        ttk.Label(main_frame, text="示例: https://xiaozhi.me/api/developers/generate-license?token=YOUR_TOKEN&seed=00:00:00:00:00:00", 
                 foreground="gray", font=("Arial", 8)).pack(anchor=tk.W, pady=(0, 15))
        
        # 网格布局配置
        grid_frame = ttk.LabelFrame(main_frame, text="网格布局", padding="10")
        grid_frame.pack(fill=tk.X, pady=(0, 15))
        
        grid_config_frame = ttk.Frame(grid_frame)
        grid_config_frame.pack(fill=tk.X)
        
        ttk.Label(grid_config_frame, text="行数:").grid(row=0, column=0, sticky=tk.W, padx=(0, 10))
        rows_spinbox = ttk.Spinbox(grid_config_frame, from_=1, to=16, textvariable=grid_rows_var, width=10)
        rows_spinbox.grid(row=0, column=1, padx=(0, 20))
        
        ttk.Label(grid_config_frame, text="列数:").grid(row=0, column=2, sticky=tk.W, padx=(0, 10))
        cols_spinbox = ttk.Spinbox(grid_config_frame, from_=1, to=16, textvariable=grid_cols_var, width=10)
        cols_spinbox.grid(row=0, column=3)
        
        # 预设布局按钮
        preset_frame = ttk.Frame(grid_frame)
        preset_frame.pack(fill=tk.X, pady=(10, 0))
        
        ttk.Label(preset_frame, text="预设布局:").pack(side=tk.LEFT)
        
        def set_preset(rows, cols):
            grid_rows_var.set(str(rows))
            grid_cols_var.set(str(cols))
        
        ttk.Button(preset_frame, text="4×4", command=lambda: set_preset(4, 4)).pack(side=tk.LEFT, padx=(10, 5))
        ttk.Button(preset_frame, text="8×1", command=lambda: set_preset(8, 1)).pack(side=tk.LEFT, padx=5)
        ttk.Button(preset_frame, text="2×8", command=lambda: set_preset(2, 8)).pack(side=tk.LEFT, padx=5)
        ttk.Button(preset_frame, text="1×16", command=lambda: set_preset(1, 16)).pack(side=tk.LEFT, padx=5)
        
        # 全屏配置
        fullscreen_check = ttk.Checkbutton(main_frame, text="启动时全屏显示", variable=fullscreen_var)
        fullscreen_check.pack(anchor=tk.W, pady=(0, 20))
        
        # 按钮框架
        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill=tk.X, pady=(20, 0))
        
        result = [False]  # 使用列表来存储结果，避免闭包问题
        
        def save_and_close():
            license_url = license_url_var.get().strip()
            if not license_url:
                messagebox.showerror("错误", "请输入授权链接")
                return
            
            try:
                rows = int(grid_rows_var.get())
                cols = int(grid_cols_var.get())
                if rows < 1 or rows > 16 or cols < 1 or cols > 16:
                    messagebox.showerror("错误", "行数和列数必须在1-16之间")
                    return
            except ValueError:
                messagebox.showerror("错误", "行数和列数必须是有效数字")
                return
            
            # 保存配置
            self.license_url.set(license_url)
            self.grid_rows = rows
            self.grid_cols = cols
            self.fullscreen_on_startup = fullscreen_var.get()
            
            # 保存到文件
            config = {
                'license_url': license_url,
                'grid_rows': rows,
                'grid_cols': cols,
                'fullscreen_on_startup': fullscreen_var.get()
            }
            
            try:
                with open(self.config_file, 'w', encoding='utf-8') as f:
                    json.dump(config, f, ensure_ascii=False, indent=2)
                result[0] = True
                dialog.destroy()
            except Exception as e:
                messagebox.showerror("错误", f"保存配置失败: {str(e)}")
        
        def cancel_and_close():
            dialog.destroy()
        
        ttk.Button(button_frame, text="保存配置", command=save_and_close).pack(side=tk.RIGHT, padx=(10, 0))
        ttk.Button(button_frame, text="取消", command=cancel_and_close).pack(side=tk.RIGHT)
        
        # 设置焦点到授权链接输入框
        license_entry.focus_set()
        
        # 等待对话框关闭
        dialog.wait_window()
        
        return result[0]

def main():
    root = tk.Tk()
    app = None
    
    # 创建自定义样式
    style = ttk.Style()
    style.configure('Port.TFrame', background='lightgray')
    style.configure('Success.TFrame', background='lightgreen')
    style.configure('Working.TFrame', background='lightblue')
    style.configure('Error.TFrame', background='lightcoral')
    style.configure('Normal.TFrame', background='white')
    
    try:
        app = ESP32ProductionTool(root)
        # 检查窗口是否还存在（可能在配置阶段被销毁）
        if root.winfo_exists():
            root.mainloop()
    except tk.TclError:
        # 窗口已被销毁，正常退出
        pass
    except Exception as e:
        print(f"程序运行出错: {e}")
    finally:
        # 确保资源被正确清理
        try:
            if app:
                app.cleanup()
        except:
            pass
        # 确保窗口被正确关闭
        try:
            if root.winfo_exists():
                root.destroy()
        except:
            pass

if __name__ == "__main__":
    main()