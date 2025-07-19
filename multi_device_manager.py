#!/usr/bin/env python3
import os
import logging
import threading
import time
from typing import Dict, List, Optional, Callable, Tuple, Any
from queue import Queue, Empty
import traceback
from datetime import datetime
from collections import defaultdict

from device_instance import DeviceInstance, DeviceStatus, ProcessingPhase, TimingStatistics
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

class StatisticsCollector:
    """全局统计数据收集器"""
    
    def __init__(self):
        self.device_stats: Dict[str, TimingStatistics] = {}
        self.completion_history: List[Dict[str, Any]] = []
        self._lock = threading.RLock()
        self._max_history_size = 1000  # 最多保留1000条历史记录
    
    def record_device_completion(self, device: DeviceInstance) -> None:
        """记录设备完成的统计信息"""
        try:
            with self._lock:
                # 记录设备统计
                self.device_stats[device.device_id] = device.timing_stats
                
                # 添加到历史记录
                completion_record = {
                    "device_id": device.device_id,
                    "port": device.port,
                    "mac_address": device.mac_address,
                    "status": device.status.value,
                    "completed_at": datetime.now(),
                    "timing_stats": device.timing_stats.to_dict(),
                    "success": device.status == DeviceStatus.COMPLETED
                }
                
                self.completion_history.append(completion_record)
                
                # 限制历史记录大小
                if len(self.completion_history) > self._max_history_size:
                    self.completion_history = self.completion_history[-self._max_history_size:]
                    
        except Exception as e:
            logger.error(f"记录设备完成统计失败: {e}")
    
    def get_average_times(self) -> Dict[str, float]:
        """获取各阶段平均处理时间"""
        try:
            with self._lock:
                if not self.device_stats:
                    return {}
                
                # 计算各阶段总时间和设备数量
                phase_totals = defaultdict(float)
                phase_counts = defaultdict(int)
                
                for device_id, timing_stats in self.device_stats.items():
                    for phase in [ProcessingPhase.MAC_ACQUISITION, ProcessingPhase.DEVICE_REGISTRATION,
                                ProcessingPhase.CONFIG_UPDATE, ProcessingPhase.FIRMWARE_BUILD,
                                ProcessingPhase.FIRMWARE_FLASH]:
                        duration = timing_stats.get_phase_duration(phase)
                        if duration is not None:
                            phase_totals[phase] += duration
                            phase_counts[phase] += 1
                
                # 计算平均值
                averages = {}
                for phase in phase_totals:
                    if phase_counts[phase] > 0:
                        averages[phase] = phase_totals[phase] / phase_counts[phase]
                
                # 计算总平均时间
                total_times = []
                for timing_stats in self.device_stats.values():
                    total_duration = timing_stats.get_total_duration()
                    if total_duration is not None:
                        total_times.append(total_duration)
                
                if total_times:
                    averages["total"] = sum(total_times) / len(total_times)
                
                return averages
        except Exception as e:
            logger.error(f"计算平均时间失败: {e}")
            return {}
    
    def get_success_rate(self) -> float:
        """获取成功率"""
        try:
            with self._lock:
                if not self.completion_history:
                    return 0.0
                
                successful_count = sum(1 for record in self.completion_history if record["success"])
                return (successful_count / len(self.completion_history)) * 100
        except Exception as e:
            logger.error(f"计算成功率失败: {e}")
            return 0.0
    
    def get_performance_summary(self) -> Dict[str, Any]:
        """获取性能摘要"""
        try:
            with self._lock:
                summary = {
                    "total_devices_processed": len(self.completion_history),
                    "successful_devices": sum(1 for record in self.completion_history if record["success"]),
                    "failed_devices": sum(1 for record in self.completion_history if not record["success"]),
                    "success_rate": self.get_success_rate(),
                    "average_times": self.get_average_times(),
                    "fastest_device": None,
                    "slowest_device": None
                }
                
                # 找出最快和最慢的设备
                if self.completion_history:
                    successful_records = [r for r in self.completion_history if r["success"]]
                    if successful_records:
                        def get_total_time(record):
                            return record["timing_stats"].get("total_duration", float('inf'))
                        
                        fastest = min(successful_records, key=get_total_time)
                        slowest = max(successful_records, key=get_total_time)
                        
                        summary["fastest_device"] = {
                            "device_id": fastest["device_id"],
                            "duration": fastest["timing_stats"].get("total_duration")
                        }
                        summary["slowest_device"] = {
                            "device_id": slowest["device_id"],
                            "duration": slowest["timing_stats"].get("total_duration")
                        }
                
                return summary
        except Exception as e:
            logger.error(f"生成性能摘要失败: {e}")
            return {}
    
    def get_recent_completions(self, count: int = 10) -> List[Dict[str, Any]]:
        """获取最近完成的设备记录"""
        try:
            with self._lock:
                return self.completion_history[-count:] if self.completion_history else []
        except Exception as e:
            logger.error(f"获取最近完成记录失败: {e}")
            return []
    
    def clear_statistics(self) -> None:
        """清空统计数据"""
        try:
            with self._lock:
                self.device_stats.clear()
                self.completion_history.clear()
                logger.info("统计数据已清空")
        except Exception as e:
            logger.error(f"清空统计数据失败: {e}")

class QueueProcessor:
    """队列处理器，负责顺序处理设备队列"""
    
    def __init__(self, device_manager):
        self.device_manager = device_manager
        self.processing_thread: Optional[threading.Thread] = None
        self.stop_event = threading.Event()
        self.current_device_id: Optional[str] = None
        self.is_running = False
    
    def start_processing(self):
        """启动队列处理线程"""
        if self.is_running:
            logger.warning("队列处理器已在运行中")
            return
        
        self.stop_event.clear()
        self.is_running = True
        self.processing_thread = threading.Thread(
            target=self._processing_loop,
            name="queue_processor",
            daemon=True
        )
        self.processing_thread.start()
        logger.info("队列处理器已启动")
    
    def stop_processing(self):
        """停止队列处理"""
        if not self.is_running:
            return
        
        self.stop_event.set()
        self.is_running = False
        
        # 等待处理线程结束
        if self.processing_thread and self.processing_thread.is_alive():
            self.processing_thread.join(timeout=5.0)
        
        self.current_device_id = None
        logger.info("队列处理器已停止")
    
    def _processing_loop(self):
        """队列处理主循环"""
        logger.info("开始队列处理循环")
        
        while not self.stop_event.is_set():
            try:
                # 从队列获取下一个设备
                device_id = self.device_manager.device_queue.get(timeout=1.0)
                
                if self.stop_event.is_set():
                    # 如果收到停止信号，将设备放回队列
                    self.device_manager.device_queue.put(device_id)
                    break
                
                device = self.device_manager.devices.get(device_id)
                if not device or not device.can_start():
                    logger.warning(f"队列中的设备 {device_id} 无法处理，跳过")
                    continue
                
                # 处理设备
                self.current_device_id = device_id
                logger.info(f"开始处理队列中的设备: {device_id}")
                
                try:
                    success = self.device_manager._process_device(device, auto_all_steps=True)
                    if success:
                        logger.info(f"设备 {device_id} 处理完成")
                    else:
                        logger.warning(f"设备 {device_id} 处理失败")
                except Exception as e:
                    logger.error(f"处理设备 {device_id} 时发生异常: {e}")
                    device.set_error(f"处理异常: {e}")
                
                self.current_device_id = None
                
            except Empty:
                # 队列为空，继续循环等待
                continue
            except Exception as e:
                logger.error(f"队列处理循环异常: {e}")
                time.sleep(1.0)
        
        logger.info("队列处理循环结束")

class MultiDeviceManager:
    """多设备管理器，协调多个设备实例的排队处理"""
    
    def __init__(self, idf_path: str = None):
        self.devices: Dict[str, DeviceInstance] = {}
        self.idf_path = idf_path
        
        # 设备处理队列
        self.device_queue = Queue()
        self.queue_processor = QueueProcessor(self)
        
        # 统计收集器
        self.statistics = StatisticsCollector()
        
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
        
        # NVS直写模式相关
        self.nvs_direct_mode = False
        self.universal_firmware_path = None

        # 烧录选项
        self.erase_before_flash = False
        
        logger.info(f"多设备管理器初始化完成，使用队列处理机制")
    
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
            
            # 如果设备正在处理中，停止处理
            if self.queue_processor.current_device_id == device_id:
                logger.warning(f"设备 {device_id} 正在处理中，将被中断")
                device.cancel()
            
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
            active_devices = []
            current_device_id = self.queue_processor.current_device_id
            if current_device_id and current_device_id in self.devices:
                active_devices.append(self.devices[current_device_id])
            return active_devices
    
    def set_nvs_direct_mode(self, enabled: bool):
        """设置NVS直写模式"""
        with self._lock:
            self.nvs_direct_mode = enabled
            logger.info(f"NVS直写模式: {'启用' if enabled else '禁用'}")

    def set_erase_before_flash(self, enabled: bool):
        """设置烧录前是否擦除内存"""
        with self._lock:
            self.erase_before_flash = enabled
            logger.info(f"烧录前擦除: {'启用' if enabled else '禁用'}")

    def get_device_mac_only(self, device_id: str) -> bool:
        """单独获取设备的MAC地址"""
        with self._lock:
            device = self.devices.get(device_id)
            if not device:
                logger.error(f"设备 {device_id} 不存在")
                return False

            # 检查设备是否正在处理中
            if device.is_active():
                logger.warning(f"设备 {device_id} 正在处理中，无法单独获取MAC地址")
                return False

            # 检查端口是否可用
            if not device.port:
                logger.error(f"设备 {device_id} 端口信息不完整")
                return False

            logger.info(f"开始获取设备 {device_id} 的MAC地址...")

            # 更新设备状态
            device.update_status(DeviceStatus.MAC_GETTING, 50, "正在获取MAC地址...")

            try:
                # 获取MAC地址
                mac_address = self._get_device_mac_safe(device)

                if mac_address:
                    device.set_mac_address(mac_address)
                    device.update_status(DeviceStatus.IDLE, 100, "MAC地址获取成功")
                    logger.info(f"设备 {device_id} MAC地址获取成功: {mac_address}")
                    return True
                else:
                    device.set_error("获取MAC地址失败")
                    device.update_status(DeviceStatus.FAILED, 0, "MAC地址获取失败")
                    logger.error(f"设备 {device_id} MAC地址获取失败")
                    return False

            except Exception as e:
                device.set_error(f"获取MAC地址异常: {e}")
                device.update_status(DeviceStatus.FAILED, 0, "MAC地址获取异常")
                logger.error(f"设备 {device_id} MAC地址获取异常: {e}")
                return False
    
    def set_universal_firmware_path(self, path: str):
        """设置通用固件路径"""
        with self._lock:
            if path and os.path.exists(path):
                self.universal_firmware_path = path
                logger.info(f"通用固件路径已设置: {path}")
            else:
                logger.warning(f"通用固件路径无效: {path}")
                self.universal_firmware_path = None
    
    def add_device_to_queue(self, device_id: str) -> bool:
        """将设备添加到处理队列"""
        with self._lock:
            device = self.devices.get(device_id)
            if not device or not device.can_start():
                logger.warning(f"设备 {device_id} 无法添加到队列")
                return False
            
            # 设置设备为排队状态
            device.update_status(DeviceStatus.IDLE, 0, "等待处理...")
            self.device_queue.put(device_id)
            logger.info(f"设备 {device_id} 已添加到处理队列")
            return True

    def start_device_processing(self, device_id: str, auto_all_steps: bool = True) -> bool:
        """开始处理单个设备"""
        with self._lock:
            device = self.devices.get(device_id)
            if not device or not device.can_start():
                logger.warning(f"设备 {device_id} 无法开始处理")
                return False
            
            # 如果队列处理器正在运行，直接添加到队列
            if self.queue_processor.is_running:
                return self.add_device_to_queue(device_id)
            
            # 否则启动队列处理器并添加设备
            if self.add_device_to_queue(device_id):
                self.queue_processor.start_processing()
                return True
            
            return False

    def start_all_devices_processing(self, auto_all_steps: bool = True) -> int:
        """开始处理所有设备（排队模式）"""
        with self._lock:
            # 停止现有的处理
            if self.queue_processor.is_running:
                self.queue_processor.stop_processing()
            
            # 清空队列
            while not self.device_queue.empty():
                try:
                    self.device_queue.get_nowait()
                except Empty:
                    break
            
            # 将所有可处理的设备添加到队列
            queued_count = 0
            for device_id, device in self.devices.items():
                if device.can_start():
                    if self.add_device_to_queue(device_id):
                        queued_count += 1
            
            logger.info(f"开始批量处理，共 {queued_count} 个设备加入队列")
            
            # 启动队列处理器
            if queued_count > 0:
                self.queue_processor.start_processing()
            
            return queued_count

    def stop_device_processing(self, device_id: str) -> bool:
        """停止处理单个设备"""
        with self._lock:
            device = self.devices.get(device_id)
            if not device:
                return False
            
            device.cancel()
            
            # 如果是当前正在处理的设备，中断处理
            if self.queue_processor.current_device_id == device_id:
                logger.info(f"中断当前处理的设备: {device_id}")
                # 队列处理器会自动继续处理下一个设备
            
            logger.info(f"设备 {device_id} 处理已停止")
            return True

    def stop_all_processing(self):
        """停止所有设备处理"""
        with self._lock:
            # 停止队列处理器
            self.queue_processor.stop_processing()
            
            # 清空队列
            while not self.device_queue.empty():
                try:
                    device_id = self.device_queue.get_nowait()
                    device = self.devices.get(device_id)
                    if device:
                        device.update_status(DeviceStatus.IDLE, 0, "处理已取消")
                except Empty:
                    break
            
            logger.info("所有设备处理已停止，队列已清空")
    
    def wait_for_completion(self, timeout: Optional[float] = None) -> bool:
        """等待所有设备完成处理"""
        start_time = time.time()
        
        while True:
            # 检查队列是否为空且没有设备在处理
            if self.device_queue.empty() and not self.queue_processor.current_device_id:
                logger.info("所有设备处理完成")
                return True
            
            # 检查超时
            if timeout and (time.time() - start_time) > timeout:
                logger.warning("等待设备完成超时")
                return False
            
            time.sleep(0.5)

    def _process_device(self, device: DeviceInstance, auto_all_steps: bool = True):
        """处理单个设备的完整流程"""
        try:
            device.update_status(DeviceStatus.DETECTING, 0, "开始处理设备")
            
            if auto_all_steps:
                # 执行完整流程（在项目根目录中）
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
            
            # 记录设备完成统计
            self.statistics.record_device_completion(device)
            
            return success
            
        except Exception as e:
            device.set_error(f"设备处理异常: {e}")
            logger.error(f"设备 {device.device_id} 处理异常: {e}")
            with self._lock:
                self.failed_devices += 1
            
            # 记录失败设备统计
            self.statistics.record_device_completion(device)
            return False
    
    def _execute_full_process(self, device: DeviceInstance) -> bool:
        """执行完整的设备处理流程"""
        try:
            # 1. 获取MAC地址
            device.update_status(DeviceStatus.MAC_GETTING, 10, "正在获取MAC地址...")
            device.start_processing_phase(ProcessingPhase.MAC_ACQUISITION)
            
            mac_address = self._get_device_mac_safe(device)
            if not mac_address:
                device.set_error("获取MAC地址失败", failed_phase=ProcessingPhase.MAC_ACQUISITION)
                return False
            
            device.set_mac_address(mac_address)
            device.end_processing_phase(ProcessingPhase.MAC_ACQUISITION)
            
            # 2. 注册设备
            device.update_status(DeviceStatus.REGISTERING, 30, "正在注册设备...")
            device.start_processing_phase(ProcessingPhase.DEVICE_REGISTRATION)
            
            client_id = self._register_device_safe(device)
            if not client_id:
                device.set_error("设备注册失败", failed_phase=ProcessingPhase.DEVICE_REGISTRATION)
                return False
            
            device.set_client_info(client_id)
            device.end_processing_phase(ProcessingPhase.DEVICE_REGISTRATION)
            
            # 检查是否启用NVS直写模式
            if self.nvs_direct_mode:
                device.update_status(DeviceStatus.CONFIG_UPDATING, 50, "NVS直写模式: 跳过配置更新和编译...")
                device.log_message("已启用NVS直写模式，跳过配置更新和固件编译")
                
                # 直接进入烧录阶段
                device.update_status(DeviceStatus.FLASHING, 70, "正在烧录通用固件...")
                device.start_processing_phase(ProcessingPhase.FIRMWARE_FLASH)
                
                if not self._flash_universal_firmware_safe(device):
                    device.set_error("烧录通用固件失败", failed_phase=ProcessingPhase.FIRMWARE_FLASH)
                    return False
                
                device.end_processing_phase(ProcessingPhase.FIRMWARE_FLASH)
                
                # NVS直写CLIENT_ID
                device.update_status(DeviceStatus.FLASHING, 90, "正在写入CLIENT_ID到NVS...")
                
                if not self._write_client_id_to_nvs_safe(device):
                    device.set_error("写入CLIENT_ID到NVS失败")
                    return False
                
            else:
                # 标准模式：3. 更新配置
                device.update_status(DeviceStatus.CONFIG_UPDATING, 50, "正在更新配置...")
                device.start_processing_phase(ProcessingPhase.CONFIG_UPDATE)
                
                if not self._update_config_safe(device):
                    device.set_error("更新配置失败", failed_phase=ProcessingPhase.CONFIG_UPDATE)
                    return False
                
                device.end_processing_phase(ProcessingPhase.CONFIG_UPDATE)
                
                # 4. 编译固件
                device.update_status(DeviceStatus.BUILDING, 70, "正在编译固件...")
                device.start_processing_phase(ProcessingPhase.FIRMWARE_BUILD)
                
                if not self._build_firmware_safe(device):
                    device.set_error("编译固件失败", failed_phase=ProcessingPhase.FIRMWARE_BUILD)
                    return False
                
                device.end_processing_phase(ProcessingPhase.FIRMWARE_BUILD)
                
                # 5. 烧录固件
                device.update_status(DeviceStatus.FLASHING, 90, "正在烧录固件...")
                device.start_processing_phase(ProcessingPhase.FIRMWARE_FLASH)
                
                if not self._flash_firmware_safe(device):
                    device.set_error("烧录固件失败", failed_phase=ProcessingPhase.FIRMWARE_FLASH)
                    return False
                
                device.end_processing_phase(ProcessingPhase.FIRMWARE_FLASH)
            
            # 完成处理
            device.complete_processing()
            return True
            
        except Exception as e:
            device.set_error(f"处理过程中发生异常: {e}")
            logger.error(f"设备 {device.device_id} 处理异常: {e}")
            return False
    
    def _get_device_mac_safe(self, device: DeviceInstance) -> Optional[str]:
        """安全获取设备MAC地址"""
        try:
            # 保存当前工作目录
            original_cwd = os.getcwd()
            
            # 切换到项目根目录执行MAC获取，避免工作目录影响
            project_root = os.path.dirname(os.path.abspath(__file__))
            os.chdir(project_root)
            
            try:
                mac_address = get_device_mac(device.port)
                if mac_address:
                    device.log_message(f"获取到设备MAC地址: {mac_address}")
                return mac_address
            finally:
                # 恢复原工作目录
                os.chdir(original_cwd)
                
        except Exception as e:
            device.log_message(f"获取MAC地址异常: {e}", logging.ERROR)
            logger.error(f"获取设备 {device.device_id} MAC地址失败: {e}")
            import traceback
            logger.debug(traceback.format_exc())
            return None
    
    def _register_device_safe(self, device: DeviceInstance) -> Optional[str]:
        """安全注册设备"""
        try:
            config = device.get_device_config()
            result = register_device(
                device.mac_address,
                config["client_type"],
                config["device_name"],
                config["device_version"]
            )
            # register_device返回(client_id, bind_key)，我们只需要client_id
            if result and isinstance(result, tuple):
                return result[0]  # 只返回client_id
            return result
        except Exception as e:
            device.log_message(f"注册设备异常: {e}", logging.ERROR)
            return None
    
    def _update_config_safe(self, device: DeviceInstance) -> bool:
        """安全更新配置"""
        try:
            # 直接在项目根目录中更新配置
            return update_config(device.client_id)
        except Exception as e:
            device.log_message(f"更新配置异常: {e}", logging.ERROR)
            return False
    
    def _build_firmware_safe(self, device: DeviceInstance) -> bool:
        """安全编译固件"""
        try:
            # 定义进度回调
            def progress_callback(line):
                device.log_message(line.strip())
            
            # 直接在项目根目录中编译
            return build_firmware(self.idf_path, skip_clean=True, progress_callback=progress_callback)
        except Exception as e:
            device.log_message(f"编译固件异常: {e}", logging.ERROR)
            return False
    
    def _flash_firmware_safe(self, device: DeviceInstance) -> bool:
        """安全烧录固件"""
        try:
            # 定义进度回调
            def progress_callback(line):
                device.log_message(line.strip())
            
            # 直接在项目根目录中烧录
            return flash_firmware(device.port, self.idf_path, progress_callback=progress_callback)
        except Exception as e:
            device.log_message(f"烧录固件异常: {e}", logging.ERROR)
            return False
    
    def _flash_universal_firmware_safe(self, device: DeviceInstance) -> bool:
        """安全烧录通用固件"""
        try:
            if not self.universal_firmware_path:
                device.log_message("错误: 未设置通用固件路径", logging.ERROR)
                return False
            
            if not os.path.exists(self.universal_firmware_path):
                device.log_message(f"错误: 通用固件文件不存在: {self.universal_firmware_path}", logging.ERROR)
                return False
            
            device.log_message(f"开始烧录通用固件: {self.universal_firmware_path}")
            
            # 导入esptool
            try:
                import esptool
            except ImportError:
                device.log_message("错误: 缺少esptool依赖，请安装: pip install esptool", logging.ERROR)
                return False
            
            # 检查是否为合并固件文件
            firmware_name = os.path.basename(self.universal_firmware_path)
            
            if "merged" in firmware_name.lower():
                # 合并固件，直接烧录到0x0
                device.log_message("检测到合并固件，执行单文件烧录")
                cmd_args = [
                    '--chip', 'auto',
                    '--port', device.port,
                    '--baud', '921600',
                    'write_flash',
                    '0x0', self.universal_firmware_path
                ]
            else:
                # 检查是否需要分区烧录
                firmware_dir = os.path.dirname(self.universal_firmware_path)
                bootloader_path = os.path.join(firmware_dir, "bootloader", "bootloader.bin")
                partition_table_path = os.path.join(firmware_dir, "partition_table", "partition-table.bin")
                ota_data_path = os.path.join(firmware_dir, "ota_data_initial.bin")
                srmodels_path = os.path.join(firmware_dir, "srmodels", "srmodels.bin")
                
                if (os.path.exists(bootloader_path) and 
                    os.path.exists(partition_table_path) and 
                    os.path.exists(ota_data_path)):
                    
                    # 分区烧录模式
                    device.log_message("使用分区烧录模式")
                    cmd_args = [
                        '--chip', 'auto',
                        '--port', device.port,
                        '--baud', '921600',
                        'write_flash',
                        '--flash_mode', 'dio',
                        '--flash_freq', '80m',
                        '0x0', bootloader_path,
                        '0x8000', partition_table_path,
                        '0xd000', ota_data_path
                    ]
                    
                    # 添加语音模型和主固件
                    if os.path.exists(srmodels_path):
                        cmd_args.extend(['0x10000', srmodels_path])
                        cmd_args.extend(['0x400000', self.universal_firmware_path])
                    else:
                        cmd_args.extend(['0x10000', self.universal_firmware_path])
                        
                else:
                    # 回退到单文件烧录
                    device.log_message("回退到单文件烧录模式")
                    cmd_args = [
                        '--chip', 'auto',
                        '--port', device.port,
                        '--baud', '921600',
                        'write_flash',
                        '0x0', self.universal_firmware_path
                    ]
            
            device.log_message(f"执行烧录命令: esptool.py {' '.join(cmd_args)}")

            # 根据选项决定是否执行擦除操作
            if self.erase_before_flash:
                device.log_message("正在擦除Flash...")
                erase_cmd_args = [
                    '--chip', 'auto',
                    '--port', device.port,
                    '--baud', '921600',
                    'erase_flash'
                ]

                device.log_message(f"执行擦除命令: esptool.py {' '.join(erase_cmd_args)}")

                try:
                    esptool.main(erase_cmd_args)
                    device.log_message("Flash擦除完成")
                except SystemExit as e:
                    if e.code == 0:
                        device.log_message("Flash擦除完成")
                    else:
                        device.log_message(f"Flash擦除失败，退出代码: {e.code}", logging.ERROR)
                        return False
                except Exception as e:
                    device.log_message(f"Flash擦除异常: {e}", logging.ERROR)
                    return False

                device.log_message("Flash擦除完成，开始烧录通用固件...")
            else:
                device.log_message("跳过Flash擦除，直接开始烧录通用固件...")
            
            # 执行烧录
            try:
                esptool.main(cmd_args)
                device.log_message("通用固件烧录成功")
                return True
            except SystemExit as e:
                if e.code == 0:
                    device.log_message("通用固件烧录成功")
                    return True
                else:
                    device.log_message(f"通用固件烧录失败，退出代码: {e.code}", logging.ERROR)
                    return False
                    
        except Exception as e:
            device.log_message(f"烧录通用固件异常: {e}", logging.ERROR)
            return False
    
    def _find_idf_python(self) -> Optional[str]:
        """查找ESP-IDF的Python环境"""
        try:
            # 方法1: 查找.espressif目录下的Python环境
            espressif_base = os.path.expanduser("~/.espressif")
            if os.name == 'nt':  # Windows
                espressif_base = os.path.expanduser("~\\.espressif")
            
            if os.path.exists(espressif_base):
                python_env_dir = os.path.join(espressif_base, "python_env")
                if os.path.exists(python_env_dir):
                    # 查找匹配的IDF版本环境
                    for env_name in os.listdir(python_env_dir):
                        if "idf" in env_name.lower():
                            if os.name == 'nt':  # Windows
                                python_exe = os.path.join(python_env_dir, env_name, "Scripts", "python.exe")
                            else:  # Linux/Mac
                                python_exe = os.path.join(python_env_dir, env_name, "bin", "python")
                            
                            if os.path.exists(python_exe):
                                return python_exe
            
            # 方法2: 尝试使用IDF_PYTHON环境变量
            idf_python = os.environ.get('IDF_PYTHON')
            if idf_python and os.path.exists(idf_python):
                return idf_python
            
            # 方法3: 尝试从IDF_PATH推断Python路径
            if self.idf_path:
                # 在Windows上，ESP-IDF通常有install.bat设置的Python环境
                if os.name == 'nt':
                    possible_paths = [
                        os.path.join(os.path.dirname(self.idf_path), "python_env", "idf5.4_py3.10_env", "Scripts", "python.exe"),
                        os.path.join(os.path.dirname(self.idf_path), "python_env", "idf_py3.10_env", "Scripts", "python.exe"),
                        os.path.join(self.idf_path, "tools", "python_env", "Scripts", "python.exe")
                    ]
                else:
                    possible_paths = [
                        os.path.join(os.path.dirname(self.idf_path), "python_env", "idf5.4_py3.10_env", "bin", "python"),
                        os.path.join(os.path.dirname(self.idf_path), "python_env", "idf_py3.10_env", "bin", "python"),
                        os.path.join(self.idf_path, "tools", "python_env", "bin", "python")
                    ]
                
                for path in possible_paths:
                    if os.path.exists(path):
                        return path
            
            # 方法4: 回退到系统Python，但这可能不工作
            import sys
            return sys.executable
            
        except Exception as e:
            logger.error(f"查找ESP-IDF Python环境失败: {e}")
            return None

    def _write_client_id_to_nvs_safe(self, device: DeviceInstance) -> bool:
        """安全写入CLIENT_ID到NVS分区"""
        try:
            if not device.client_id:
                device.log_message("错误: 设备CLIENT_ID为空", logging.ERROR)
                return False
            
            device.log_message(f"开始写入CLIENT_ID到NVS: {device.client_id}")
            
            # 导入esptool
            try:
                import esptool
            except ImportError:
                device.log_message("错误: 缺少esptool依赖，请安装: pip install esptool", logging.ERROR)
                return False
            
            # 使用NVS分区工具写入
            try:
                # 创建临时NVS CSV文件
                import tempfile
                import csv
                
                with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as csv_file:
                    csv_writer = csv.writer(csv_file)
                    csv_writer.writerow(['key', 'type', 'encoding', 'value'])
                    csv_writer.writerow(['websocket', 'namespace', '', ''])
                    csv_writer.writerow(['client_id', 'data', 'string', device.client_id])
                    csv_temp_path = csv_file.name
                
                # 生成NVS分区bin文件
                with tempfile.NamedTemporaryFile(suffix='.bin', delete=False) as bin_file:
                    nvs_bin_path = bin_file.name
                
                # 查找nvs_partition_gen.py
                idf_path = self.idf_path or os.environ.get('IDF_PATH')
                if not idf_path:
                    device.log_message("错误: 未设置IDF_PATH", logging.ERROR)
                    return False
                
                nvs_gen_path = os.path.join(idf_path, 'components', 'nvs_flash', 'nvs_partition_generator', 'nvs_partition_gen.py')
                
                if not os.path.exists(nvs_gen_path):
                    device.log_message(f"错误: 找不到NVS分区生成工具: {nvs_gen_path}", logging.ERROR)
                    return False
                
                # 生成NVS分区
                import subprocess
                
                # 查找ESP-IDF的Python环境
                idf_python = self._find_idf_python()
                if not idf_python:
                    device.log_message("错误: 找不到ESP-IDF的Python环境", logging.ERROR)
                    return False
                
                gen_cmd = [
                    idf_python, nvs_gen_path,
                    'generate', csv_temp_path, nvs_bin_path, '0x4000'
                ]
                
                device.log_message(f"生成NVS分区: {' '.join(gen_cmd)}")
                result = subprocess.run(gen_cmd, capture_output=True, text=True)
                
                if result.returncode != 0:
                    device.log_message(f"生成NVS分区失败: {result.stderr}", logging.ERROR)
                    return False
                
                # 烧录NVS分区到设备
                flash_cmd = [
                    '--chip', 'auto',
                    '--port', device.port,
                    '--baud', '921600',
                    'write_flash',
                    '0x9000', nvs_bin_path  # NVS分区通常在0x9000地址
                ]
                
                device.log_message(f"写入NVS分区: esptool.py {' '.join(flash_cmd)}")
                
                try:
                    esptool.main(flash_cmd)
                    device.log_message(f"CLIENT_ID已成功写入NVS: {device.client_id}")
                    return True
                except SystemExit as e:
                    if e.code == 0:
                        device.log_message(f"CLIENT_ID已成功写入NVS: {device.client_id}")
                        return True
                    else:
                        device.log_message(f"写入NVS失败，退出代码: {e.code}", logging.ERROR)
                        return False
                        
                # 清理临时文件
                finally:
                    try:
                        os.unlink(csv_temp_path)
                        os.unlink(nvs_bin_path)
                    except:
                        pass
                        
            except Exception as e:
                device.log_message(f"NVS操作异常: {e}", logging.ERROR)
                return False
                
        except Exception as e:
            device.log_message(f"写入CLIENT_ID到NVS异常: {e}", logging.ERROR)
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
            queued_count = self.device_queue.qsize()
            return {
                "total": self.total_devices,
                "completed": self.completed_devices,
                "failed": self.failed_devices,
                "active": active_count,
                "queued": queued_count,
                "pending": self.total_devices - self.completed_devices - self.failed_devices - active_count - queued_count
            }
    
    def get_queue_status(self) -> Dict[str, any]:
        """获取队列状态信息"""
        with self._lock:
            return {
                "queue_enabled": self.queue_processor.is_running,
                "queue_size": self.device_queue.qsize(),
                "current_processing": self.queue_processor.current_device_id,
                "active_count": len(self.get_active_devices())
            }
    
    def cleanup(self):
        """清理资源"""
        logger.info("正在清理多设备管理器资源...")
        
        # 停止所有处理
        self.stop_all_processing()
        
        logger.info("多设备管理器资源清理完成")
    
    def get_timing_statistics(self) -> Dict[str, Any]:
        """获取详细的时间统计信息"""
        try:
            return {
                "average_times": self.statistics.get_average_times(),
                "performance_summary": self.statistics.get_performance_summary(),
                "recent_completions": self.statistics.get_recent_completions(20)
            }
        except Exception as e:
            logger.error(f"获取时间统计失败: {e}")
            return {}
    
    def get_performance_summary(self) -> Dict[str, Any]:
        """获取性能摘要"""
        try:
            return self.statistics.get_performance_summary()
        except Exception as e:
            logger.error(f"获取性能摘要失败: {e}")
            return {}
    
    def get_average_processing_times(self) -> Dict[str, float]:
        """获取平均处理时间"""
        try:
            return self.statistics.get_average_times()
        except Exception as e:
            logger.error(f"获取平均处理时间失败: {e}")
            return {}
    
    def get_success_rate(self) -> float:
        """获取成功率"""
        try:
            return self.statistics.get_success_rate()
        except Exception as e:
            logger.error(f"获取成功率失败: {e}")
            return 0.0
    
    def clear_statistics(self):
        """清空统计数据"""
        try:
            self.statistics.clear_statistics()
            logger.info("统计数据已清空")
        except Exception as e:
            logger.error(f"清空统计数据失败: {e}")
    
    def get_device_processing_time(self, device_id: str) -> Optional[str]:
        """获取指定设备的处理时间摘要"""
        try:
            device = self.get_device(device_id)
            if device:
                return device.get_processing_time_summary()
            return None
        except Exception as e:
            logger.error(f"获取设备处理时间失败: {e}")
            return None
    
    def retry_device_from_phase(self, device_id: str, phase: str) -> bool:
        """从指定阶段重试设备"""
        with self._lock:
            device = self.devices.get(device_id)
            if not device:
                logger.warning(f"设备 {device_id} 不存在")
                return False
            
            if not device.is_failed():
                logger.warning(f"设备 {device_id} 未失败，无需重试")
                return False
            
            if not device.can_retry_from_phase(phase):
                logger.warning(f"设备 {device_id} 无法从阶段 {phase} 重试")
                return False
            
            # 重置设备状态并从指定阶段开始
            device.reset_from_phase(phase)
            logger.info(f"设备 {device_id} 已重置，将从阶段 {phase} 重试")
            
            # 开始处理
            return self.start_device_processing(device_id)
    
    def retry_device_full(self, device_id: str) -> bool:
        """完整重试设备"""
        return self.retry_device_from_phase(device_id, ProcessingPhase.MAC_ACQUISITION)
    
    def retry_device_current_phase(self, device_id: str) -> bool:
        """重试设备当前失败阶段"""
        with self._lock:
            device = self.devices.get(device_id)
            if not device or not device.is_failed():
                return False
            
            failed_phase = device.failed_phase
            if not failed_phase:
                return self.retry_device_full(device_id)
            
            return self.retry_device_from_phase(device_id, failed_phase)
    
    def skip_phase_and_continue(self, device_id: str, skip_phase: str) -> bool:
        """跳过阶段并继续处理"""
        with self._lock:
            device = self.devices.get(device_id)
            if not device or not device.is_failed():
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
                skip_index = phase_order.index(skip_phase)
                if skip_index >= len(phase_order) - 1:
                    logger.warning(f"无法跳过阶段 {skip_phase}，这是最后一个阶段")
                    return False
                
                next_phase = phase_order[skip_index + 1]
                device.log_message(f"跳过阶段: {skip_phase}，从 {next_phase} 继续")
                return self.retry_device_from_phase(device_id, next_phase)
                
            except ValueError:
                logger.error(f"未知阶段: {skip_phase}")
                return False
    
    def get_retry_options(self, device_id: str) -> List[Dict[str, str]]:
        """获取设备的重试选项"""
        device = self.devices.get(device_id)
        if not device:
            return []
        
        return device.get_retry_options()
    
    def get_device_error_details(self, device_id: str) -> Optional[Dict[str, Any]]:
        """获取设备的详细错误信息"""
        device = self.devices.get(device_id)
        if not device:
            return None
        
        return device.get_error_details()
    
    def reset_device(self, device_id: str) -> bool:
        """重置设备状态"""
        with self._lock:
            device = self.devices.get(device_id)
            if not device:
                return False
            
            device.reset()
            logger.info(f"设备 {device_id} 状态已重置")
            return True
    
    def retry_all_failed_devices(self) -> int:
        """重试所有失败的设备"""
        with self._lock:
            retried_count = 0
            for device_id, device in self.devices.items():
                if device.is_failed():
                    if self.retry_device_current_phase(device_id):
                        retried_count += 1
            
            logger.info(f"已重试 {retried_count} 个失败设备")
            return retried_count
    
    def retry_failed_devices_by_phase(self, failed_phase: str) -> int:
        """重试指定阶段失败的设备"""
        with self._lock:
            retried_count = 0
            for device_id, device in self.devices.items():
                if device.is_failed() and device.failed_phase == failed_phase:
                    if self.retry_device_current_phase(device_id):
                        retried_count += 1
            
            logger.info(f"已重试 {retried_count} 个在阶段 {failed_phase} 失败的设备")
            return retried_count
    
    def __enter__(self):
        """上下文管理器入口"""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """上下文管理器出口"""
        self.cleanup() 