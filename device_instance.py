#!/usr/bin/env python3
import os
import logging
import threading
from enum import Enum
from datetime import datetime
from typing import Optional, Dict, Any, Callable
import json

logger = logging.getLogger("shaolu.device")

class DeviceStatus(Enum):
    """设备状态枚举"""
    IDLE = "idle"                    # 空闲
    DETECTING = "detecting"          # 检测中
    MAC_GETTING = "mac_getting"      # 获取MAC中
    REGISTERING = "registering"      # 注册中
    CONFIG_UPDATING = "config_updating"  # 更新配置中
    BUILDING = "building"            # 编译中
    FLASHING = "flashing"           # 烧录中
    COMPLETED = "completed"         # 完成
    FAILED = "failed"               # 失败
    CANCELLED = "cancelled"         # 取消

class DeviceInstance:
    """设备实例类，封装单个设备的所有状态和操作"""
    
    def __init__(self, device_id: str, port: str = None):
        self.device_id = device_id
        self.port = port
        self.mac_address: Optional[str] = None
        self.client_id: Optional[str] = None
        self.bind_key: Optional[str] = None
        
        # 工作目录相关
        self.workspace_path: Optional[str] = None
        
        # 设备配置
        self.client_type: str = "esp32"
        self.device_name: str = ""
        self.device_version: str = "1.0.0"
        
        # 状态管理
        self.status = DeviceStatus.IDLE
        self.progress: int = 0
        self.progress_message: str = "就绪"
        self.error_message: str = ""
        
        # 时间戳
        self.created_at = datetime.now()
        self.started_at: Optional[datetime] = None
        self.completed_at: Optional[datetime] = None
        
        # 线程安全锁
        self._lock = threading.RLock()
        
        # 回调函数
        self.status_callback: Optional[Callable] = None
        self.progress_callback: Optional[Callable] = None
        self.log_callback: Optional[Callable] = None
        
        logger.info(f"设备实例创建: {self.device_id} (端口: {self.port})")
    
    def set_callbacks(self, status_callback=None, progress_callback=None, log_callback=None):
        """设置回调函数"""
        with self._lock:
            if status_callback:
                self.status_callback = status_callback
            if progress_callback:
                self.progress_callback = progress_callback
            if log_callback:
                self.log_callback = log_callback
    
    def update_status(self, status: DeviceStatus, progress: int = None, message: str = None):
        """更新设备状态"""
        with self._lock:
            old_status = self.status
            self.status = status
            
            if progress is not None:
                self.progress = max(0, min(100, progress))
            
            if message is not None:
                self.progress_message = message
            
            # 记录状态变化时间
            if status != old_status:
                if status == DeviceStatus.DETECTING and old_status == DeviceStatus.IDLE:
                    self.started_at = datetime.now()
                elif status in [DeviceStatus.COMPLETED, DeviceStatus.FAILED, DeviceStatus.CANCELLED]:
                    self.completed_at = datetime.now()
            
            logger.debug(f"设备 {self.device_id} 状态更新: {old_status.value} -> {status.value}")
            
            # 调用状态回调
            if self.status_callback:
                try:
                    self.status_callback(self, old_status, status)
                except Exception as e:
                    logger.error(f"状态回调执行失败: {e}")
    
    def update_progress(self, progress: int, message: str = None):
        """更新进度"""
        with self._lock:
            self.progress = max(0, min(100, progress))
            if message:
                self.progress_message = message
            
            # 调用进度回调
            if self.progress_callback:
                try:
                    self.progress_callback(self, self.progress, self.progress_message)
                except Exception as e:
                    logger.error(f"进度回调执行失败: {e}")
    
    def log_message(self, message: str, level: int = logging.INFO):
        """记录日志消息"""
        # 添加设备标识前缀
        prefixed_message = f"[{self.device_id}] {message}"
        logger.log(level, prefixed_message)
        
        # 调用日志回调
        if self.log_callback:
            try:
                self.log_callback(self, prefixed_message, level)
            except Exception as e:
                logger.error(f"日志回调执行失败: {e}")
    
    def set_error(self, error_message: str, status: DeviceStatus = DeviceStatus.FAILED):
        """设置错误状态"""
        with self._lock:
            self.error_message = error_message
            self.update_status(status, message=f"错误: {error_message}")
            self.log_message(f"发生错误: {error_message}", logging.ERROR)
    
    def set_mac_address(self, mac_address: str):
        """设置MAC地址"""
        with self._lock:
            self.mac_address = mac_address
            if not self.device_name:
                # 生成默认设备名称
                self.device_name = f"小乔设备-{mac_address.replace(':', '')[-4:]}"
            self.log_message(f"MAC地址已获取: {mac_address}")
    
    def set_client_info(self, client_id: str, bind_key: str = None):
        """设置客户端信息"""
        with self._lock:
            self.client_id = client_id
            if bind_key:
                self.bind_key = bind_key
            self.log_message(f"设备注册成功 - Client ID: {client_id}")
            if bind_key:
                self.log_message(f"绑定码: {bind_key}")
    
    def set_workspace(self, workspace_path: str):
        """设置工作目录"""
        with self._lock:
            self.workspace_path = workspace_path
            self.log_message(f"工作目录已设置: {workspace_path}")
    
    def get_device_config(self) -> Dict[str, Any]:
        """获取设备配置"""
        with self._lock:
            return {
                "client_type": self.client_type,
                "device_name": self.device_name,
                "device_version": self.device_version
            }
    
    def set_device_config(self, client_type: str = None, device_name: str = None, device_version: str = None):
        """设置设备配置"""
        with self._lock:
            if client_type:
                self.client_type = client_type
            if device_name:
                self.device_name = device_name
            if device_version:
                self.device_version = device_version
    
    def get_status_info(self) -> Dict[str, Any]:
        """获取状态信息"""
        with self._lock:
            duration = None
            if self.started_at:
                end_time = self.completed_at or datetime.now()
                duration = (end_time - self.started_at).total_seconds()
            
            return {
                "device_id": self.device_id,
                "port": self.port,
                "mac_address": self.mac_address,
                "client_id": self.client_id,
                "bind_key": self.bind_key,
                "status": self.status.value,
                "progress": self.progress,
                "progress_message": self.progress_message,
                "error_message": self.error_message,
                "duration": duration,
                "created_at": self.created_at.isoformat(),
                "started_at": self.started_at.isoformat() if self.started_at else None,
                "completed_at": self.completed_at.isoformat() if self.completed_at else None,
                "workspace_path": self.workspace_path
            }
    
    def is_active(self) -> bool:
        """检查设备是否处于活动状态"""
        with self._lock:
            return self.status not in [DeviceStatus.IDLE, DeviceStatus.COMPLETED, DeviceStatus.FAILED, DeviceStatus.CANCELLED]
    
    def is_completed(self) -> bool:
        """检查设备是否已完成"""
        with self._lock:
            return self.status == DeviceStatus.COMPLETED
    
    def is_failed(self) -> bool:
        """检查设备是否失败"""
        with self._lock:
            return self.status == DeviceStatus.FAILED
    
    def can_start(self) -> bool:
        """检查是否可以开始处理"""
        with self._lock:
            return self.status in [DeviceStatus.IDLE, DeviceStatus.FAILED] and self.port is not None
    
    def cancel(self):
        """取消设备操作"""
        with self._lock:
            if self.is_active():
                self.update_status(DeviceStatus.CANCELLED, message="操作已取消")
                self.log_message("设备操作已取消")
    
    def reset(self):
        """重置设备状态"""
        with self._lock:
            self.status = DeviceStatus.IDLE
            self.progress = 0
            self.progress_message = "就绪"
            self.error_message = ""
            self.started_at = None
            self.completed_at = None
            self.log_message("设备状态已重置")
    
    def save_to_workspace(self):
        """保存设备信息到工作目录"""
        if not self.workspace_path:
            return False
            
        try:
            device_info_file = os.path.join(self.workspace_path, "device_status.json")
            status_info = self.get_status_info()
            status_info.update(self.get_device_config())
            
            with open(device_info_file, "w", encoding="utf-8") as f:
                json.dump(status_info, f, ensure_ascii=False, indent=4)
            
            return True
        except Exception as e:
            self.log_message(f"保存设备信息失败: {e}", logging.ERROR)
            return False
    
    def load_from_workspace(self, workspace_path: str):
        """从工作目录加载设备信息"""
        try:
            device_info_file = os.path.join(workspace_path, "device_status.json")
            if not os.path.exists(device_info_file):
                return False
                
            with open(device_info_file, "r", encoding="utf-8") as f:
                info = json.load(f)
            
            with self._lock:
                self.workspace_path = workspace_path
                self.mac_address = info.get("mac_address")
                self.client_id = info.get("client_id") 
                self.bind_key = info.get("bind_key")
                self.client_type = info.get("client_type", "esp32")
                self.device_name = info.get("device_name", "")
                self.device_version = info.get("device_version", "1.0.0")
                
                # 恢复时间戳
                if info.get("created_at"):
                    self.created_at = datetime.fromisoformat(info["created_at"])
                if info.get("started_at"):
                    self.started_at = datetime.fromisoformat(info["started_at"])
                if info.get("completed_at"):
                    self.completed_at = datetime.fromisoformat(info["completed_at"])
            
            self.log_message(f"从工作目录加载设备信息: {workspace_path}")
            return True
            
        except Exception as e:
            self.log_message(f"加载设备信息失败: {e}", logging.ERROR)
            return False
    
    def __str__(self):
        """字符串表示"""
        return f"DeviceInstance({self.device_id}, {self.port}, {self.status.value})"
    
    def __repr__(self):
        """调试表示"""
        return (f"DeviceInstance(device_id='{self.device_id}', port='{self.port}', "
                f"status={self.status.value}, progress={self.progress}%)") 