#!/usr/bin/env python3
"""
多设备管理UI - 简化版本
这是多实例模式的用户界面，支持多个ESP32设备的并发烧录
"""

import os
import sys
import logging
from datetime import datetime

# 检查PySide6依赖
try:
    from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                                QPushButton, QLabel, QTableWidget, QTableWidgetItem, 
                                QProgressBar, QTextEdit, QComboBox, QGroupBox, QFormLayout,
                                QMessageBox, QInputDialog)
    from PySide6.QtCore import Qt, Signal, Slot, QTimer
    from PySide6.QtGui import QFont
    HAS_PYSIDE6 = True
except ImportError:
    HAS_PYSIDE6 = False
    print("错误: 缺少PySide6依赖，请安装: pip install PySide6")
    sys.exit(1)

# 导入我们的多设备模块
from multi_device_manager import MultiDeviceManager
from device_instance import DeviceInstance, DeviceStatus

logger = logging.getLogger("shaolu.multi_ui")

class SimpleMultiDeviceUI(QMainWindow):
    """简化的多设备管理界面"""
    
    def __init__(self):
        super().__init__()
        
        # 初始化设备管理器（已改为排队机制）
        self.device_manager = MultiDeviceManager()
        self.device_manager.set_callbacks(
            device_status_callback=self.on_device_status_changed,
            log_callback=self.on_device_log
        )
        
        self.setup_ui()
        self.setup_connections()
        
        # 定时器更新界面
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.update_device_table)
        self.update_timer.start(1000)  # 每秒更新
        
        logger.info("多设备管理界面初始化完成")
    
    def setup_ui(self):
        """设置界面"""
        self.setWindowTitle("小乔智能设备 - 多设备自动烧录工具")
        self.setMinimumSize(1000, 700)
        
        # 主布局
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QVBoxLayout(main_widget)
        
        # 工具栏
        toolbar = self.create_toolbar()
        layout.addLayout(toolbar)
        
        # 设备表格
        self.device_table = QTableWidget()
        self.setup_device_table()
        layout.addWidget(self.device_table)
        
        # 日志区域
        log_group = QGroupBox("运行日志")
        log_layout = QVBoxLayout(log_group)
        
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setFont(QFont("Consolas", 9))
        self.log_text.setMaximumHeight(200)
        log_layout.addWidget(self.log_text)
        
        layout.addWidget(log_group)
        
        # 状态栏
        self.statusBar().showMessage("就绪")
    
    def create_toolbar(self):
        """创建工具栏"""
        layout = QHBoxLayout()
        
        # 检测设备按钮
        detect_btn = QPushButton("自动检测设备")
        detect_btn.clicked.connect(self.auto_detect_devices)
        layout.addWidget(detect_btn)
        
        # 添加设备按钮
        add_btn = QPushButton("手动添加设备")
        add_btn.clicked.connect(self.add_device_manually)
        layout.addWidget(add_btn)
        
        # 开始全部按钮
        start_all_btn = QPushButton("开始全部")
        start_all_btn.setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold;")
        start_all_btn.clicked.connect(self.start_all_devices)
        layout.addWidget(start_all_btn)
        
        # 停止全部按钮
        stop_all_btn = QPushButton("停止全部")
        stop_all_btn.clicked.connect(self.stop_all_devices)
        layout.addWidget(stop_all_btn)
        
        layout.addStretch()
        
        # 统计信息
        self.stats_label = QLabel("设备总数: 0")
        layout.addWidget(self.stats_label)
        
        return layout
    
    def setup_device_table(self):
        """设置设备表格"""
        headers = ["设备ID", "端口", "状态", "进度", "MAC地址", "操作"]
        self.device_table.setColumnCount(len(headers))
        self.device_table.setHorizontalHeaderLabels(headers)
        
        # 设置列宽
        self.device_table.setColumnWidth(0, 120)  # 设备ID
        self.device_table.setColumnWidth(1, 80)   # 端口
        self.device_table.setColumnWidth(2, 100)  # 状态
        self.device_table.setColumnWidth(3, 200)  # 进度
        self.device_table.setColumnWidth(4, 150)  # MAC地址
        self.device_table.setColumnWidth(5, 120)  # 操作
    
    def setup_connections(self):
        """设置信号连接"""
        pass
    
    @Slot()
    def auto_detect_devices(self):
        """自动检测设备"""
        try:
            ports = self.device_manager.auto_detect_devices()
            
            if not ports:
                QMessageBox.information(self, "检测结果", "未检测到可用设备")
                return
            
            added_count = 0
            for port in ports:
                # 先处理端口名称中的特殊字符
                safe_port = port.replace('/', '_').replace('\\', '_')
                device_id = f"Device_{safe_port}"
                
                if not self.device_manager.get_device(device_id):
                    self.device_manager.add_device(device_id, port)
                    added_count += 1
            
            self.update_device_table()
            self.log_message(f"自动检测并添加了 {added_count} 个设备")
            
            if added_count > 0:
                QMessageBox.information(self, "检测完成", f"成功添加 {added_count} 个设备")
            else:
                QMessageBox.information(self, "检测完成", "所有设备都已存在")
                
        except Exception as e:
            logger.error(f"自动检测设备失败: {e}")
            QMessageBox.critical(self, "错误", f"自动检测设备失败: {e}")
    
    @Slot()
    def add_device_manually(self):
        """手动添加设备"""
        port, ok = QInputDialog.getText(self, "添加设备", "请输入设备端口 (如 COM3):")
        if ok and port:
            # 先处理端口名称中的特殊字符
            safe_port = port.replace('/', '_').replace('\\', '_')
            device_id = f"Device_{safe_port}"
            
            if self.device_manager.get_device(device_id):
                QMessageBox.warning(self, "警告", "该设备已存在")
                return
            
            self.device_manager.add_device(device_id, port)
            self.update_device_table()
            self.log_message(f"手动添加设备: {device_id} (端口: {port})")
    
    @Slot()
    def start_all_devices(self):
        """开始处理所有设备（排队模式）"""
        try:
            queued_count = self.device_manager.start_all_devices_processing()
            self.log_message(f"开始批量处理（排队机制），共 {queued_count} 个设备加入队列")
            
            if queued_count == 0:
                QMessageBox.information(self, "提示", "没有可以开始的设备")
            else:
                QMessageBox.information(self, "队列开始", f"已启动排队处理，共 {queued_count} 个设备将依次处理")
            
        except Exception as e:
            logger.error(f"开始全部设备失败: {e}")
            QMessageBox.critical(self, "错误", f"开始全部设备失败: {e}")
    
    @Slot()
    def stop_all_devices(self):
        """停止处理所有设备"""
        try:
            self.device_manager.stop_all_processing()
            self.log_message("已停止所有设备处理并清空队列")
            
        except Exception as e:
            logger.error(f"停止全部设备失败: {e}")
            QMessageBox.critical(self, "错误", f"停止全部设备失败: {e}")
    
    def start_device(self, device_id: str):
        """开始处理单个设备"""
        if self.device_manager.start_device_processing(device_id):
            self.log_message(f"开始处理设备: {device_id}")
        else:
            QMessageBox.warning(self, "警告", f"无法开始处理设备 {device_id}")
    
    def stop_device(self, device_id: str):
        """停止处理单个设备"""
        if self.device_manager.stop_device_processing(device_id):
            self.log_message(f"停止处理设备: {device_id}")
        else:
            QMessageBox.warning(self, "警告", f"无法停止处理设备 {device_id}")
    
    @Slot()
    def update_device_table(self):
        """更新设备表格"""
        devices = self.device_manager.get_all_devices()
        
        # 更新表格行数
        self.device_table.setRowCount(len(devices))
        
        for row, device in enumerate(devices):
            # 设备ID
            self.device_table.setItem(row, 0, QTableWidgetItem(device.device_id))
            
            # 端口
            self.device_table.setItem(row, 1, QTableWidgetItem(device.port or "未知"))
            
            # 状态
            status_text = self.get_status_text(device.status)
            self.device_table.setItem(row, 2, QTableWidgetItem(status_text))
            
            # 进度条
            progress_bar = QProgressBar()
            progress_bar.setMinimum(0)
            progress_bar.setMaximum(100)
            progress_bar.setValue(device.progress)
            progress_bar.setFormat(f"{device.progress}% - {device.progress_message}")
            self.device_table.setCellWidget(row, 3, progress_bar)
            
            # MAC地址
            self.device_table.setItem(row, 4, QTableWidgetItem(device.mac_address or "未获取"))
            
            # 操作按钮
            action_widget = self.create_action_widget(device.device_id)
            self.device_table.setCellWidget(row, 5, action_widget)
        
        # 更新统计信息
        stats = self.device_manager.get_statistics()
        queue_status = self.device_manager.get_queue_status()
        
        # 显示队列状态
        if queue_status['queue_enabled']:
            stats_text = f"总计: {stats['total']} | 排队: {stats['queued']} | 活动: {stats['active']} | 完成: {stats['completed']} | 失败: {stats['failed']}"
            if queue_status['current_processing']:
                stats_text += f" | 当前: {queue_status['current_processing']}"
        else:
            stats_text = f"总计: {stats['total']} | 活动: {stats['active']} | 完成: {stats['completed']} | 失败: {stats['failed']}"
        
        self.stats_label.setText(stats_text)
    
    def create_action_widget(self, device_id: str):
        """创建操作按钮组件"""
        widget = QWidget()
        layout = QHBoxLayout(widget)
        layout.setContentsMargins(2, 2, 2, 2)
        
        # 开始按钮
        start_btn = QPushButton("开始")
        start_btn.setMaximumWidth(40)
        start_btn.clicked.connect(lambda: self.start_device(device_id))
        layout.addWidget(start_btn)
        
        # 停止按钮
        stop_btn = QPushButton("停止")
        stop_btn.setMaximumWidth(40)
        stop_btn.clicked.connect(lambda: self.stop_device(device_id))
        layout.addWidget(stop_btn)
        
        return widget
    
    def get_status_text(self, status: DeviceStatus) -> str:
        """获取状态显示文本"""
        status_map = {
            DeviceStatus.IDLE: "空闲",
            DeviceStatus.DETECTING: "检测中",
            DeviceStatus.MAC_GETTING: "获取MAC",
            DeviceStatus.REGISTERING: "注册中",
            DeviceStatus.CONFIG_UPDATING: "更新配置",
            DeviceStatus.BUILDING: "编译中",
            DeviceStatus.FLASHING: "烧录中",
            DeviceStatus.COMPLETED: "完成",
            DeviceStatus.FAILED: "失败",
            DeviceStatus.CANCELLED: "已取消"
        }
        return status_map.get(status, "未知")
    
    # 回调方法
    def on_device_status_changed(self, device: DeviceInstance, old_status: DeviceStatus, new_status: DeviceStatus):
        """设备状态变化回调"""
        logger.debug(f"设备 {device.device_id} 状态: {old_status.value} -> {new_status.value}")
    
    def on_device_log(self, device: DeviceInstance, message: str, level: int):
        """设备日志回调"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {message}"
        
        # 添加到日志文本
        self.log_text.append(formatted_message)
        # 自动滚动到底部
        self.log_text.verticalScrollBar().setValue(
            self.log_text.verticalScrollBar().maximum()
        )
    
    def log_message(self, message: str):
        """记录日志消息"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] [UI] {message}"
        
        logger.info(message)
        self.log_text.append(formatted_message)
        self.log_text.verticalScrollBar().setValue(
            self.log_text.verticalScrollBar().maximum()
        )
    
    def closeEvent(self, event):
        """窗口关闭事件"""
        try:
            self.log_message("正在关闭应用程序...")
            
            # 停止所有处理
            self.device_manager.stop_all_processing()
            
            # 等待完成
            self.device_manager.wait_for_completion(timeout=5)
            
            # 清理资源
            self.device_manager.cleanup()
            
            event.accept()
            
        except Exception as e:
            logger.error(f"关闭应用程序时发生错误: {e}")
            event.accept()

def run_multi_device_ui():
    """运行多设备管理界面"""
    if not HAS_PYSIDE6:
        print("错误: 需要安装PySide6库")
        return 1
    
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    
    # 创建并显示主窗口
    window = SimpleMultiDeviceUI()
    window.show()
    
    return app.exec()

if __name__ == "__main__":
    # 设置日志级别
    logging.basicConfig(level=logging.INFO)
    
    # 运行应用
    sys.exit(run_multi_device_ui()) 