#!/usr/bin/env python3
import os
import logging
import threading
from enum import Enum
from datetime import datetime
from typing import Optional, Dict, Any, Callable, List
from dataclasses import dataclass, field
import json

logger = logging.getLogger("shaolu.device")

@dataclass
class TimingStatistics:
    """设备处理时间统计类"""
    phase_times: Dict[str, Dict[str, Optional[datetime]]] = field(default_factory=dict)
    total_start_time: Optional[datetime] = None
    total_end_time: Optional[datetime] = None
    
    def start_phase(self, phase_name: str) -> None:
        """开始记录某个阶段的时间"""
        if phase_name not in self.phase_times:
            self.phase_times[phase_name] = {"start": None, "end": None}
        self.phase_times[phase_name]["start"] = datetime.now()
        
        # 如果是第一个阶段，记录总开始时间
        if self.total_start_time is None:
            self.total_start_time = self.phase_times[phase_name]["start"]
    
    def end_phase(self, phase_name: str) -> None:
        """结束记录某个阶段的时间"""
        if phase_name in self.phase_times:
            self.phase_times[phase_name]["end"] = datetime.now()
    
    def get_phase_duration(self, phase_name: str) -> Optional[float]:
        """获取某个阶段的持续时间（秒）"""
        if phase_name not in self.phase_times:
            return None
        
        phase_data = self.phase_times[phase_name]
        if phase_data["start"] is None:
            return None
        
        end_time = phase_data["end"] or datetime.now()
        return (end_time - phase_data["start"]).total_seconds()
    
    def get_total_duration(self) -> Optional[float]:
        """获取总处理时间（秒）"""
        if self.total_start_time is None:
            return None
        
        end_time = self.total_end_time or datetime.now()
        return (end_time - self.total_start_time).total_seconds()
    
    def get_current_phase(self) -> Optional[str]:
        """获取当前正在进行的阶段"""
        for phase_name, times in self.phase_times.items():
            if times["start"] is not None and times["end"] is None:
                return phase_name
        return None
    
    def complete_processing(self) -> None:
        """完成所有处理，记录总结束时间"""
        self.total_end_time = datetime.now()
        
        # 结束任何未完成的阶段
        current_phase = self.get_current_phase()
        if current_phase:
            self.end_phase(current_phase)
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典格式"""
        result = {
            "total_start_time": self.total_start_time.isoformat() if self.total_start_time else None,
            "total_end_time": self.total_end_time.isoformat() if self.total_end_time else None,
            "total_duration": self.get_total_duration(),
            "phases": {}
        }
        
        for phase_name, times in self.phase_times.items():
            result["phases"][phase_name] = {
                "start_time": times["start"].isoformat() if times["start"] else None,
                "end_time": times["end"].isoformat() if times["end"] else None,
                "duration": self.get_phase_duration(phase_name)
            }
        
        return result
    
    def format_duration(self, duration: Optional[float]) -> str:
        """格式化持续时间为可读字符串"""
        if duration is None:
            return "未知"
        
        if duration < 1:
            return f"{duration * 1000:.0f}ms"
        elif duration < 60:
            return f"{duration:.1f}s"
        else:
            minutes = int(duration // 60)
            seconds = duration % 60
            return f"{minutes}m{seconds:.1f}s"

# 定义标准处理阶段
class ProcessingPhase:
    """处理阶段常量定义"""
    MAC_ACQUISITION = "mac_acquisition"
    DEVICE_REGISTRATION = "device_registration"
    CONFIG_UPDATE = "config_update"
    FIRMWARE_BUILD = "firmware_build"
    FIRMWARE_FLASH = "firmware_flash"

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
        self.client_type: str = "esp32s3"
        self.device_name: str = ""
        self.device_version: str = "1.0.0"
        
        # 状态管理
        self.status = DeviceStatus.IDLE
        self.progress: int = 0
        self.progress_message: str = "就绪"
        self.error_message: str = ""
        
        # 错误处理和重试相关
        self.failed_phase: Optional[str] = None
        self.retry_count: int = 0
        self.last_error_details: Dict[str, Any] = {}
        
        # 时间戳
        self.created_at = datetime.now()
        self.started_at: Optional[datetime] = None
        self.completed_at: Optional[datetime] = None
        
        # 时间统计
        self.timing_stats = TimingStatistics()
        
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
    
    def set_error(self, error_message: str, status: DeviceStatus = DeviceStatus.FAILED, failed_phase: str = None):
        """设置错误状态"""
        with self._lock:
            self.error_message = error_message
            self.failed_phase = failed_phase or self.timing_stats.get_current_phase()
            
            # 记录详细错误信息
            self.last_error_details = {
                "error_message": error_message,
                "failed_phase": self.failed_phase,
                "error_time": datetime.now().isoformat(),
                "retry_count": self.retry_count,
                "current_status": self.status.value
            }
            
            self.update_status(status, message=f"错误: {error_message}")
            self.log_message(f"发生错误: {error_message} (阶段: {self.failed_phase})", logging.ERROR)
    
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
                "failed_phase": self.failed_phase,
                "retry_count": self.retry_count,
                "last_error_details": self.last_error_details,
                "duration": duration,
                "created_at": self.created_at.isoformat(),
                "started_at": self.started_at.isoformat() if self.started_at else None,
                "completed_at": self.completed_at.isoformat() if self.completed_at else None,
                "workspace_path": self.workspace_path,
                "timing_statistics": self.timing_stats.to_dict()
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
            self.failed_phase = None
            self.retry_count = 0
            self.last_error_details = {}
            self.started_at = None
            self.completed_at = None
            self.log_message("设备状态已重置")
    
    def can_retry_from_phase(self, phase: str) -> bool:
        """检查是否可以从指定阶段重试"""
        with self._lock:
            if not self.is_failed():
                return False
            
            # 定义阶段顺序
            phase_order = [
                ProcessingPhase.MAC_ACQUISITION,
                ProcessingPhase.DEVICE_REGISTRATION,
                ProcessingPhase.CONFIG_UPDATE,
                ProcessingPhase.FIRMWARE_BUILD,
                ProcessingPhase.FIRMWARE_FLASH
            ]
            
            try:
                current_phase_index = phase_order.index(self.failed_phase) if self.failed_phase else 0
                retry_phase_index = phase_order.index(phase)
                return retry_phase_index <= current_phase_index
            except ValueError:
                return True  # 如果阶段不在列表中，允许重试
    
    def reset_from_phase(self, phase: str):
        """从指定阶段重置设备状态"""
        with self._lock:
            self.status = DeviceStatus.IDLE
            self.error_message = ""
            self.failed_phase = None
            self.retry_count += 1
            self.progress = 0
            self.progress_message = f"准备从{phase}阶段重试"
            
            # 清理指定阶段之后的数据
            if phase == ProcessingPhase.MAC_ACQUISITION:
                self.mac_address = None
                self.client_id = None
                self.bind_key = None
                self.device_name = ""
            elif phase == ProcessingPhase.DEVICE_REGISTRATION:
                self.client_id = None
                self.bind_key = None
            
            self.log_message(f"设备已重置，准备从{phase}阶段重试 (第{self.retry_count}次重试)")
    
    def get_retry_options(self) -> List[Dict[str, str]]:
        """获取可用的重试选项"""
        with self._lock:
            if not self.is_failed():
                return []
            
            options = []
            
            # 根据失败阶段提供不同的重试选项
            if self.failed_phase == ProcessingPhase.MAC_ACQUISITION:
                options.append({"action": "retry_current", "label": "重新获取MAC", "phase": ProcessingPhase.MAC_ACQUISITION})
                options.append({"action": "retry_full", "label": "完整重试", "phase": ProcessingPhase.MAC_ACQUISITION})
                
            elif self.failed_phase == ProcessingPhase.DEVICE_REGISTRATION:
                options.append({"action": "retry_current", "label": "重新注册", "phase": ProcessingPhase.DEVICE_REGISTRATION})
                options.append({"action": "retry_from_mac", "label": "从MAC获取重试", "phase": ProcessingPhase.MAC_ACQUISITION})
                options.append({"action": "retry_full", "label": "完整重试", "phase": ProcessingPhase.MAC_ACQUISITION})
                
            elif self.failed_phase == ProcessingPhase.CONFIG_UPDATE:
                options.append({"action": "retry_current", "label": "重新更新配置", "phase": ProcessingPhase.CONFIG_UPDATE})
                options.append({"action": "retry_from_register", "label": "从注册重试", "phase": ProcessingPhase.DEVICE_REGISTRATION})
                options.append({"action": "retry_full", "label": "完整重试", "phase": ProcessingPhase.MAC_ACQUISITION})
                options.append({"action": "skip_continue", "label": "跳过配置更新", "phase": ProcessingPhase.FIRMWARE_BUILD})
                
            elif self.failed_phase == ProcessingPhase.FIRMWARE_BUILD:
                options.append({"action": "retry_current", "label": "重新编译", "phase": ProcessingPhase.FIRMWARE_BUILD})
                options.append({"action": "retry_from_config", "label": "从配置更新重试", "phase": ProcessingPhase.CONFIG_UPDATE})
                options.append({"action": "retry_full", "label": "完整重试", "phase": ProcessingPhase.MAC_ACQUISITION})
                
            elif self.failed_phase == ProcessingPhase.FIRMWARE_FLASH:
                options.append({"action": "retry_current", "label": "重新烧录", "phase": ProcessingPhase.FIRMWARE_FLASH})
                options.append({"action": "retry_from_build", "label": "从编译重试", "phase": ProcessingPhase.FIRMWARE_BUILD})
                options.append({"action": "retry_full", "label": "完整重试", "phase": ProcessingPhase.MAC_ACQUISITION})
                
            else:
                # 通用失败情况
                options.append({"action": "retry_full", "label": "完整重试", "phase": ProcessingPhase.MAC_ACQUISITION})
            
            # 总是添加重置选项
            options.append({"action": "reset", "label": "重置设备", "phase": None})
            
            return options
    
    def get_error_details(self) -> Dict[str, Any]:
        """获取详细错误信息"""
        with self._lock:
            return {
                "error_message": self.error_message,
                "failed_phase": self.failed_phase,
                "retry_count": self.retry_count,
                "last_error_details": self.last_error_details.copy(),
                "current_status": self.status.value,
                "can_retry": self.is_failed(),
                "retry_options": self.get_retry_options()
            }
    
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
                self.client_type = info.get("client_type", "esp32s3")
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
    
    def start_processing_phase(self, phase_name: str) -> None:
        """开始处理阶段的时间记录"""
        try:
            with self._lock:
                self.timing_stats.start_phase(phase_name)
                self.log_message(f"开始阶段: {phase_name}")
        except Exception as e:
            logger.error(f"记录阶段开始时间失败 {phase_name}: {e}")
    
    def end_processing_phase(self, phase_name: str) -> None:
        """结束处理阶段的时间记录"""
        try:
            with self._lock:
                self.timing_stats.end_phase(phase_name)
                duration = self.timing_stats.get_phase_duration(phase_name)
                formatted_duration = self.timing_stats.format_duration(duration)
                self.log_message(f"完成阶段: {phase_name} (耗时: {formatted_duration})")
        except Exception as e:
            logger.error(f"记录阶段结束时间失败 {phase_name}: {e}")
    
    def get_processing_time_summary(self) -> str:
        """获取处理时间摘要字符串"""
        try:
            with self._lock:
                total_duration = self.timing_stats.get_total_duration()
                current_phase = self.timing_stats.get_current_phase()
                
                if total_duration is None:
                    return "未开始"
                
                formatted_total = self.timing_stats.format_duration(total_duration)
                
                if current_phase:
                    phase_duration = self.timing_stats.get_phase_duration(current_phase)
                    formatted_phase = self.timing_stats.format_duration(phase_duration)
                    return f"{formatted_total} (当前: {current_phase} - {formatted_phase})"
                else:
                    return formatted_total
        except Exception as e:
            logger.error(f"获取处理时间摘要失败: {e}")
            return "统计异常"
    
    def get_phase_durations(self) -> Dict[str, float]:
        """获取各阶段持续时间"""
        try:
            with self._lock:
                durations = {}
                for phase in [ProcessingPhase.MAC_ACQUISITION, ProcessingPhase.DEVICE_REGISTRATION,
                            ProcessingPhase.CONFIG_UPDATE, ProcessingPhase.FIRMWARE_BUILD,
                            ProcessingPhase.FIRMWARE_FLASH]:
                    duration = self.timing_stats.get_phase_duration(phase)
                    if duration is not None:
                        durations[phase] = duration
                return durations
        except Exception as e:
            logger.error(f"获取阶段持续时间失败: {e}")
            return {}
    
    def complete_processing(self) -> None:
        """完成所有处理，记录统计信息"""
        try:
            with self._lock:
                self.timing_stats.complete_processing()
                total_duration = self.timing_stats.get_total_duration()
                formatted_duration = self.timing_stats.format_duration(total_duration)
                self.log_message(f"设备处理完成，总耗时: {formatted_duration}")
        except Exception as e:
            logger.error(f"完成处理统计失败: {e}") 