#!/usr/bin/env python3
import os
import logging
import threading
import time
from typing import Dict, List, Optional, Callable, Tuple
from concurrent.futures import ThreadPoolExecutor, Future
import traceback
from datetime import datetime

from device_instance import DeviceInstance, DeviceStatus
from workspace_manager import WorkspaceManager

# 导入原有的核心函数
import sys
sys.path.append(os.path.dirname(__file__))
try:
    from shaolu_ui import (get_device_mac, register_device, update_config, 
                          build_firmware, flash_firmware, detect_ports)
except ImportError:
    # 如果无法导入，我们稍后会重新定义这些函数
    logger = logging.getLogger("shaolu.multi")
    logger.warning("无法导入shaolu_ui模块的函数，将使用本地实现")

logger = logging.getLogger("shaolu.multi")

class MultiDeviceManager:
    """多设备管理器，协调多个设备实例的并发操作"""
    
    def __init__(self, max_workers: int = 4, idf_path: str = None):
        self.devices: Dict[str, DeviceInstance] = {}
        self.workspace_manager = WorkspaceManager()
        self.max_workers = max_workers
        self.idf_path = idf_path
        
        # 线程池管理
        self.thread_pool = ThreadPoolExecutor(max_workers=max_workers, thread_name_prefix="device_worker")
        self.active_futures: Dict[str, Future] = {}
        
        # 线程安全锁
        self._lock = threading.RLock()
        
        # 全局回调函数
        self.device_status_callback: Optional[Callable] = None
        self.progress_callback: Optional[Callable] = None
        self.log_callback: Optional[Callable] = None
        
        # 统计信息
        self.total_devices = 0
        self.completed_devices = 0
        self.failed_devices = 0
        
        logger.info(f"多设备管理器初始化完成，最大并发数: {max_workers}")
    
    def set_callbacks(self, device_status_callback=None, progress_callback=None, log_callback=None):
        """设置全局回调函数"""
        with self._lock:
            if device_status_callback:
                self.device_status_callback = device_status_callback
            if progress_callback:
                self.progress_callback = progress_callback
            if log_callback:
                self.log_callback = log_callback
    
    def auto_detect_devices(self) -> List[str]:
        """自动检测连接的设备"""
        try:
            logger.info("正在自动检测设备...")
            ports = detect_ports()
            logger.info(f"检测到 {len(ports)} 个可能的设备端口")
            return ports
        except Exception as e:
            logger.error(f"自动检测设备失败: {e}")
            return []
    
    def add_device(self, device_id: str, port: str) -> DeviceInstance:
        """添加设备到管理器"""
        with self._lock:
            if device_id in self.devices:
                logger.warning(f"设备 {device_id} 已存在，将更新端口信息")
                self.devices[device_id].port = port
                return self.devices[device_id]
            
            device = DeviceInstance(device_id, port)
            
            # 设置设备回调函数
            device.set_callbacks(
                status_callback=self._on_device_status_changed,
                progress_callback=self._on_device_progress_changed,
                log_callback=self._on_device_log
            )
            
            self.devices[device_id] = device
            self.total_devices += 1
            
            logger.info(f"设备已添加: {device_id} (端口: {port})")
            return device
    
    def remove_device(self, device_id: str) -> bool:
        """移除设备"""
        with self._lock:
            if device_id not in self.devices:
                return False
            
            device = self.devices[device_id]
            
            # 取消正在进行的任务
            if device_id in self.active_futures:
                self.active_futures[device_id].cancel()
                del self.active_futures[device_id]
            
            # 清理工作目录
            if device.workspace_path:
                self.workspace_manager.cleanup_workspace(device.workspace_path)
            
            del self.devices[device_id]
            self.total_devices -= 1
            
            logger.info(f"设备已移除: {device_id}")
            return True
    
    def get_device(self, device_id: str) -> Optional[DeviceInstance]:
        """获取设备实例"""
        return self.devices.get(device_id)
    
    def get_all_devices(self) -> List[DeviceInstance]:
        """获取所有设备实例"""
        with self._lock:
            return list(self.devices.values())
    
    def get_active_devices(self) -> List[DeviceInstance]:
        """获取活动中的设备"""
        with self._lock:
            return [device for device in self.devices.values() if device.is_active()]
    
    def start_device_processing(self, device_id: str, auto_all_steps: bool = True) -> bool:
        """开始处理单个设备"""
        with self._lock:
            device = self.devices.get(device_id)
            if not device or not device.can_start():
                logger.warning(f"设备 {device_id} 无法开始处理")
                return False
            
            if device_id in self.active_futures:
                logger.warning(f"设备 {device_id} 已在处理中")
                return False
            
            # 提交任务到线程池
            future = self.thread_pool.submit(self._process_device, device, auto_all_steps)
            self.active_futures[device_id] = future
            
            logger.info(f"设备 {device_id} 开始处理")
            return True
    
    def start_all_devices_processing(self, auto_all_steps: bool = True) -> int:
        """开始处理所有设备"""
        started_count = 0
        
        for device_id, device in self.devices.items():
            if self.start_device_processing(device_id, auto_all_steps):
                started_count += 1
        
        logger.info(f"开始批量处理，共启动 {started_count} 个设备")
        return started_count
    
    def stop_device_processing(self, device_id: str) -> bool:
        """停止处理单个设备"""
        with self._lock:
            device = self.devices.get(device_id)
            if not device:
                return False
            
            device.cancel()
            
            if device_id in self.active_futures:
                self.active_futures[device_id].cancel()
                del self.active_futures[device_id]
            
            logger.info(f"设备 {device_id} 处理已停止")
            return True
    
    def stop_all_processing(self):
        """停止所有设备处理"""
        with self._lock:
            for device_id in list(self.active_futures.keys()):
                self.stop_device_processing(device_id)
            
            logger.info("所有设备处理已停止")
    
    def wait_for_completion(self, timeout: Optional[float] = None) -> bool:
        """等待所有设备完成处理"""
        start_time = time.time()
        
        while True:
            with self._lock:
                if not self.active_futures:
                    logger.info("所有设备处理完成")
                    return True
                
                # 清理已完成的future
                completed_futures = []
                for device_id, future in self.active_futures.items():
                    if future.done():
                        completed_futures.append(device_id)
                
                for device_id in completed_futures:
                    del self.active_futures[device_id]
            
            # 检查超时
            if timeout and (time.time() - start_time) > timeout:
                logger.warning("等待设备完成超时")
                return False
            
            time.sleep(0.5)
    
    def _process_device(self, device: DeviceInstance, auto_all_steps: bool = True):
        """处理单个设备的完整流程"""
        try:
            device.update_status(DeviceStatus.DETECTING, 0, "开始处理设备")
            
            # 创建工作目录
            workspace_path = self.workspace_manager.create_device_workspace(device.device_id, device.port)
            if not workspace_path:
                device.set_error("创建工作目录失败")
                return False
            
            device.set_workspace(workspace_path)
            
            if auto_all_steps:
                # 执行完整流程
                success = self._execute_full_process(device)
            else:
                # 仅准备环境
                device.update_status(DeviceStatus.IDLE, 100, "设备准备完成")
                success = True
            
            if success:
                device.update_status(DeviceStatus.COMPLETED, 100, "设备处理完成")
                with self._lock:
                    self.completed_devices += 1
            else:
                with self._lock:
                    self.failed_devices += 1
            
            # 保存设备状态
            device.save_to_workspace()
            
            return success
            
        except Exception as e:
            device.set_error(f"设备处理异常: {e}")
            logger.error(f"设备 {device.device_id} 处理异常: {e}")
            logger.debug(traceback.format_exc())
            
            with self._lock:
                self.failed_devices += 1
            return False
    
    def _execute_full_process(self, device: DeviceInstance) -> bool:
        """执行设备的完整处理流程"""
        try:
            # 1. 获取MAC地址
            device.update_status(DeviceStatus.MAC_GETTING, 10, "获取MAC地址")
            mac_address = self._get_device_mac_safe(device)
            if not mac_address:
                device.set_error("获取MAC地址失败")
                return False
            
            device.set_mac_address(mac_address)
            
            # 2. 注册设备
            device.update_status(DeviceStatus.REGISTERING, 30, "注册设备")
            client_id, bind_key = self._register_device_safe(device)
            if not client_id:
                device.set_error("注册设备失败")
                return False
            
            device.set_client_info(client_id, bind_key)
            
            # 3. 更新配置
            device.update_status(DeviceStatus.CONFIG_UPDATING, 50, "更新配置")
            if not self._update_config_safe(device):
                device.set_error("更新配置失败")
                return False
            
            # 4. 编译固件
            device.update_status(DeviceStatus.BUILDING, 70, "编译固件")
            if not self._build_firmware_safe(device):
                device.set_error("编译固件失败")
                return False
            
            # 5. 烧录固件
            device.update_status(DeviceStatus.FLASHING, 90, "烧录固件")
            if not self._flash_firmware_safe(device):
                device.set_error("烧录固件失败")
                return False
            
            return True
            
        except Exception as e:
            device.set_error(f"执行流程异常: {e}")
            return False
    
    def _get_device_mac_safe(self, device: DeviceInstance) -> Optional[str]:
        """安全获取设备MAC地址"""
        try:
            return get_device_mac(device.port)
        except Exception as e:
            device.log_message(f"获取MAC地址异常: {e}", logging.ERROR)
            return None
    
    def _register_device_safe(self, device: DeviceInstance) -> Tuple[Optional[str], Optional[str]]:
        """安全注册设备"""
        try:
            config = device.get_device_config()
            return register_device(
                device.mac_address,
                config["client_type"],
                config["device_name"],
                config["device_version"]
            )
        except Exception as e:
            device.log_message(f"注册设备异常: {e}", logging.ERROR)
            return None, None
    
    def _update_config_safe(self, device: DeviceInstance) -> bool:
        """安全更新配置"""
        try:
            # 在设备工作目录中更新配置
            return self._update_config_in_workspace(device.workspace_path, device.client_id)
        except Exception as e:
            device.log_message(f"更新配置异常: {e}", logging.ERROR)
            return False
    
    def _build_firmware_safe(self, device: DeviceInstance) -> bool:
        """安全编译固件"""
        try:
            # 在设备工作目录中编译
            return self._build_firmware_in_workspace(device.workspace_path, device)
        except Exception as e:
            device.log_message(f"编译固件异常: {e}", logging.ERROR)
            return False
    
    def _flash_firmware_safe(self, device: DeviceInstance) -> bool:
        """安全烧录固件"""
        try:
            # 在设备工作目录中烧录
            return self._flash_firmware_in_workspace(device.workspace_path, device.port, device)
        except Exception as e:
            device.log_message(f"烧录固件异常: {e}", logging.ERROR)
            return False
    
    def _update_config_in_workspace(self, workspace_path: str, client_id: str) -> bool:
        """在工作目录中更新配置"""
        try:
            # 保存当前工作目录
            original_cwd = os.getcwd()
            os.chdir(workspace_path)
            
            try:
                # 调用原有的update_config函数
                return update_config(client_id)
            finally:
                os.chdir(original_cwd)
                
        except Exception as e:
            logger.error(f"在工作目录更新配置失败: {e}")
            return False
    
    def _build_firmware_in_workspace(self, workspace_path: str, device: DeviceInstance) -> bool:
        """在工作目录中编译固件"""
        try:
            # 保存当前工作目录
            original_cwd = os.getcwd()
            os.chdir(workspace_path)
            
            try:
                # 定义进度回调
                def progress_callback(line):
                    device.log_message(line.strip())
                
                # 调用原有的build_firmware函数
                return build_firmware(self.idf_path, skip_clean=True, progress_callback=progress_callback)
            finally:
                os.chdir(original_cwd)
                
        except Exception as e:
            logger.error(f"在工作目录编译固件失败: {e}")
            return False
    
    def _flash_firmware_in_workspace(self, workspace_path: str, port: str, device: DeviceInstance) -> bool:
        """在工作目录中烧录固件"""
        try:
            # 保存当前工作目录
            original_cwd = os.getcwd()
            os.chdir(workspace_path)
            
            try:
                # 定义进度回调
                def progress_callback(line):
                    device.log_message(line.strip())
                
                # 调用原有的flash_firmware函数
                return flash_firmware(port, self.idf_path, progress_callback=progress_callback)
            finally:
                os.chdir(original_cwd)
                
        except Exception as e:
            logger.error(f"在工作目录烧录固件失败: {e}")
            return False
    
    def _on_device_status_changed(self, device: DeviceInstance, old_status: DeviceStatus, new_status: DeviceStatus):
        """设备状态变化回调"""
        if self.device_status_callback:
            try:
                self.device_status_callback(device, old_status, new_status)
            except Exception as e:
                logger.error(f"设备状态回调执行失败: {e}")
    
    def _on_device_progress_changed(self, device: DeviceInstance, progress: int, message: str):
        """设备进度变化回调"""
        if self.progress_callback:
            try:
                self.progress_callback(device, progress, message)
            except Exception as e:
                logger.error(f"设备进度回调执行失败: {e}")
    
    def _on_device_log(self, device: DeviceInstance, message: str, level: int):
        """设备日志回调"""
        if self.log_callback:
            try:
                self.log_callback(device, message, level)
            except Exception as e:
                logger.error(f"设备日志回调执行失败: {e}")
    
    def get_statistics(self) -> Dict[str, int]:
        """获取统计信息"""
        with self._lock:
            active_count = len(self.get_active_devices())
            return {
                "total": self.total_devices,
                "completed": self.completed_devices,
                "failed": self.failed_devices,
                "active": active_count,
                "pending": self.total_devices - self.completed_devices - self.failed_devices - active_count
            }
    
    def cleanup(self):
        """清理资源"""
        logger.info("正在清理多设备管理器资源...")
        
        # 停止所有处理
        self.stop_all_processing()
        
        # 等待线程池关闭
        self.thread_pool.shutdown(wait=True)
        
        # 清理工作目录
        for device in self.devices.values():
            if device.workspace_path:
                self.workspace_manager.cleanup_workspace(device.workspace_path)
        
        logger.info("多设备管理器资源清理完成")
    
    def __enter__(self):
        """上下文管理器入口"""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """上下文管理器出口"""
        self.cleanup() 