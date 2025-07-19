#!/usr/bin/env python3
import os
import sys
import logging
from datetime import datetime
from typing import Dict, List, Optional, Any

# 检查PySide6依赖
try:
    from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                                QPushButton, QLabel, QTableWidget, QTableWidgetItem, QHeaderView,
                                QProgressBar, QTextEdit, QComboBox, QLineEdit, QGroupBox, QFormLayout,
                                QSplitter, QFrame, QMessageBox, QTabWidget, QCheckBox, QSpinBox,
                                QAbstractItemView, QMenu, QInputDialog, QDialog)
    from PySide6.QtCore import Qt, Signal, Slot, QTimer
    from PySide6.QtGui import QFont, QIcon, QColor, QAction
    HAS_PYSIDE6 = True
except ImportError:
    HAS_PYSIDE6 = False
    print("错误: 缺少PySide6依赖，请安装: pip install PySide6")
    sys.exit(1)

# 导入自定义模块
from multi_device_manager import MultiDeviceManager
from device_instance import DeviceInstance, DeviceStatus
from workspace_manager import WorkspaceManager

# 移除了编译线程类，因为不再需要编译功能

# 设置日志
logger = logging.getLogger("shaolu.multi_ui")

class DeviceTableWidget(QTableWidget):
    """自定义设备列表表格"""
    
    device_selected = Signal(str)  # 设备被选中信号
    
    def __init__(self):
        super().__init__()
        self.devices: Dict[str, DeviceInstance] = {}
        self.setup_table()
    
    def setup_table(self):
        """设置表格结构"""
        # 列定义
        columns = ["设备ID", "端口", "MAC地址", "获取MAC", "状态", "进度", "处理时间", "操作"]
        self.setColumnCount(len(columns))
        self.setHorizontalHeaderLabels(columns)

        # 设置列宽
        self.setColumnWidth(0, 120)  # 设备ID
        self.setColumnWidth(1, 80)   # 端口
        self.setColumnWidth(2, 150)  # MAC地址 - 调宽一些
        self.setColumnWidth(3, 140)  # 获取MAC按钮 - 调宽一些
        self.setColumnWidth(4, 100)  # 状态
        self.setColumnWidth(5, 120)  # 进度
        self.setColumnWidth(6, 150)  # 处理时间
        self.setColumnWidth(7, 140)  # 操作

        # 表格属性
        self.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.setAlternatingRowColors(True)
        self.verticalHeader().setVisible(False)

        # 设置行高，确保按钮有足够的显示空间
        self.verticalHeader().setDefaultSectionSize(80)  # 设置默认行高为80像素 - 调高一些
        self.verticalHeader().setMinimumSectionSize(75)   # 设置最小行高为75像素 - 调高一些
        
        # 连接选择事件
        self.selectionModel().selectionChanged.connect(self.on_selection_changed)
        
        # 启用右键菜单
        self.setContextMenuPolicy(Qt.CustomContextMenu)
        self.customContextMenuRequested.connect(self.show_context_menu)
    
    def add_device(self, device: DeviceInstance):
        """添加设备到表格"""
        device_id = device.device_id
        if device_id in self.devices:
            self.update_device(device)
            return
        
        self.devices[device_id] = device
        
        # 添加新行
        row = self.rowCount()
        self.insertRow(row)
        
        # 设备ID
        self.setItem(row, 0, QTableWidgetItem(device_id))

        # 端口
        self.setItem(row, 1, QTableWidgetItem(device.port or "未知"))

        # MAC地址
        self.setItem(row, 2, QTableWidgetItem(device.mac_address or "未获取"))

        # 获取MAC按钮
        get_mac_widget = self.create_get_mac_widget(device_id)
        self.setCellWidget(row, 3, get_mac_widget)

        # 状态
        status_item = QTableWidgetItem(self.get_status_text(device.status))
        status_item.setData(Qt.UserRole, device_id)
        self.setItem(row, 4, status_item)

        # 进度条
        progress_bar = QProgressBar()
        progress_bar.setMinimum(0)
        progress_bar.setMaximum(100)
        progress_bar.setValue(device.progress)
        progress_bar.setFormat(f"{device.progress}% - {device.progress_message}")
        self.setCellWidget(row, 5, progress_bar)

        # 处理时间
        processing_time_summary = device.get_processing_time_summary() if hasattr(device, 'get_processing_time_summary') else "未开始"
        self.setItem(row, 6, QTableWidgetItem(processing_time_summary))

        # 操作按钮
        action_widget = self.create_action_widget(device_id)
        self.setCellWidget(row, 7, action_widget)

        # 设置此行的高度，确保按钮显示完整
        self.setRowHeight(row, 40)
        
        logger.debug(f"设备已添加到表格: {device_id}")
    
    def update_device(self, device: DeviceInstance):
        """更新设备信息"""
        device_id = device.device_id
        if device_id not in self.devices:
            return
        
        self.devices[device_id] = device
        
        # 查找设备行
        row = self.find_device_row(device_id)
        if row < 0:
            return
        
        # 更新MAC地址
        mac_item = self.item(row, 2)
        if mac_item:
            mac_item.setText(device.mac_address or "未获取")

        # 更新状态
        status_item = self.item(row, 4)
        if status_item:
            status_item.setText(self.get_status_text(device.status))
            # 设置状态颜色
            color = self.get_status_color(device.status)
            status_item.setBackground(color)

            # 如果是失败状态，添加特殊样式
            if device.status == DeviceStatus.FAILED:
                if hasattr(device, 'failed_phase'):
                    phase_names = {
                        "mac_acquisition": "MAC获取失败",
                        "device_registration": "注册失败",
                        "config_update": "配置失败",
                        "firmware_build": "编译失败",
                        "firmware_flash": "烧录失败"
                    }
                    phase_text = phase_names.get(device.failed_phase, "失败")
                    status_item.setText(phase_text)
                    status_item.setToolTip(f"失败阶段: {device.failed_phase}\n错误: {device.error_message}")
                else:
                    status_item.setToolTip(f"错误: {device.error_message}")

        # 更新进度
        progress_bar = self.cellWidget(row, 5)
        if isinstance(progress_bar, QProgressBar):
            progress_bar.setValue(device.progress)
            progress_bar.setFormat(f"{device.progress}% - {device.progress_message}")

        # 更新处理时间
        processing_time_summary = device.get_processing_time_summary() if hasattr(device, 'get_processing_time_summary') else "未开始"
        processing_time_item = self.item(row, 6)
        if processing_time_item:
            processing_time_item.setText(processing_time_summary)

        # 更新操作按钮（根据新状态重新创建）
        action_widget = self.create_action_widget(device_id)
        self.setCellWidget(row, 7, action_widget)
    
    def remove_device(self, device_id: str):
        """从表格移除设备"""
        if device_id not in self.devices:
            return
        
        row = self.find_device_row(device_id)
        if row >= 0:
            self.removeRow(row)
        
        del self.devices[device_id]
        logger.debug(f"设备已从表格移除: {device_id}")
    
    def find_device_row(self, device_id: str) -> int:
        """查找设备对应的行号"""
        for row in range(self.rowCount()):
            item = self.item(row, 0)
            if item and item.text() == device_id:
                return row
        return -1
    
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
    
    def get_status_color(self, status: DeviceStatus) -> QColor:
        """获取状态对应的颜色"""
        color_map = {
            DeviceStatus.IDLE: QColor(240, 240, 240),      # 灰色
            DeviceStatus.DETECTING: QColor(255, 255, 0),   # 黄色
            DeviceStatus.MAC_GETTING: QColor(255, 255, 0), # 黄色
            DeviceStatus.REGISTERING: QColor(255, 255, 0), # 黄色
            DeviceStatus.CONFIG_UPDATING: QColor(255, 255, 0), # 黄色
            DeviceStatus.BUILDING: QColor(0, 191, 255),     # 蓝色
            DeviceStatus.FLASHING: QColor(255, 165, 0),     # 橙色
            DeviceStatus.COMPLETED: QColor(144, 238, 144),  # 浅绿色
            DeviceStatus.FAILED: QColor(255, 182, 193),     # 浅红色
            DeviceStatus.CANCELLED: QColor(211, 211, 211)  # 浅灰色
        }
        return color_map.get(status, QColor(255, 255, 255))
    
    def get_button_style(self, button_type: str) -> str:
        """获取按钮样式"""
        styles = {
            "start": """
                QPushButton {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #2196F3, stop: 1 #1976D2);
                    border: 2px solid #2196F3;
                    border-radius: 6px;
                    color: white;
                    font-weight: bold;
                    font-size: 12px;
                    padding: 8px 10px;
                }
                QPushButton:hover {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #1976D2, stop: 1 #1565C0);
                    border-color: #1976D2;
                }
                QPushButton:pressed {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #1565C0, stop: 1 #0D47A1);
                    border-color: #1565C0;
                }
            """,
            "stop": """
                QPushButton {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #f44336, stop: 1 #d32f2f);
                    border: 2px solid #f44336;
                    border-radius: 6px;
                    color: white;
                    font-weight: bold;
                    font-size: 12px;
                    padding: 8px 10px;
                }
                QPushButton:hover {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #d32f2f, stop: 1 #c62828);
                    border-color: #d32f2f;
                }
                QPushButton:pressed {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #c62828, stop: 1 #b71c1c);
                    border-color: #c62828;
                }
            """,
            "retry": """
                QPushButton {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #FF9800, stop: 1 #F57C00);
                    border: 2px solid #FF9800;
                    border-radius: 6px;
                    color: white;
                    font-weight: bold;
                    font-size: 12px;
                    padding: 8px 10px;
                }
                QPushButton:hover {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #F57C00, stop: 1 #EF6C00);
                    border-color: #F57C00;
                }
                QPushButton:pressed {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #EF6C00, stop: 1 #E65100);
                    border-color: #EF6C00;
                }
            """,
            "reset": """
                QPushButton {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #9E9E9E, stop: 1 #757575);
                    border: 2px solid #9E9E9E;
                    border-radius: 6px;
                    color: white;
                    font-weight: bold;
                    font-size: 12px;
                    padding: 8px 10px;
                }
                QPushButton:hover {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #757575, stop: 1 #616161);
                    border-color: #757575;
                }
                QPushButton:pressed {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #616161, stop: 1 #424242);
                    border-color: #616161;
                }
            """,
            "more": """
                QPushButton {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #607D8B, stop: 1 #455A64);
                    border: 2px solid #607D8B;
                    border-radius: 6px;
                    color: white;
                    font-weight: bold;
                    font-size: 14px;
                    padding: 8px 8px;
                }
                QPushButton:hover {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #455A64, stop: 1 #37474F);
                    border-color: #455A64;
                }
                QPushButton:pressed {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #37474F, stop: 1 #263238);
                    border-color: #37474F;
                }
            """,
            "get_mac": """
                QPushButton {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #2196F3, stop: 1 #1976D2);
                    border: 2px solid #2196F3;
                    border-radius: 6px;
                    color: white;
                    font-weight: bold;
                    font-size: 12px;
                    padding: 8px 10px;
                }
                QPushButton:hover {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #1976D2, stop: 1 #1565C0);
                    border-color: #1976D2;
                }
                QPushButton:pressed {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #1565C0, stop: 1 #0D47A1);
                    border-color: #1565C0;
                }
            """
        }
        return styles.get(button_type, "")

    def create_get_mac_widget(self, device_id: str) -> QWidget:
        """创建获取MAC按钮组件"""
        widget = QWidget()
        layout = QHBoxLayout(widget)
        layout.setContentsMargins(2, 2, 2, 2)
        layout.setSpacing(2)

        # 创建获取MAC按钮
        get_mac_btn = QPushButton("📱 获取MAC")
        get_mac_btn.setMinimumWidth(90)  # 设置最小宽度
        get_mac_btn.setMaximumWidth(110)  # 增加最大宽度
        get_mac_btn.setMinimumHeight(38)  # 增加按钮高度
        get_mac_btn.setToolTip("单独获取此设备的MAC地址")
        get_mac_btn.setStyleSheet(self.get_button_style("get_mac"))
        get_mac_btn.clicked.connect(lambda: self.get_device_mac(device_id))

        # 将按钮添加到布局中，居中显示
        layout.addStretch()  # 左侧弹性空间
        layout.addWidget(get_mac_btn)
        layout.addStretch()  # 右侧弹性空间

        return widget

    def create_action_widget(self, device_id: str) -> QWidget:
        """创建操作按钮组件"""
        widget = QWidget()
        layout = QHBoxLayout(widget)
        layout.setContentsMargins(2, 2, 2, 2)
        
        # 根据设备状态动态创建按钮
        device = self.devices.get(device_id)
        if not device:
            return widget
        
        if device.status.value in ["idle", "completed"]:
            # 空闲或完成状态：显示开始按钮
            start_btn = QPushButton("▶️ 开始")
            start_btn.setMinimumWidth(70)   # 设置最小宽度
            start_btn.setMaximumWidth(90)   # 增加最大宽度
            start_btn.setMinimumHeight(38)  # 增加按钮高度
            start_btn.setStyleSheet(self.get_button_style("start"))
            start_btn.clicked.connect(lambda: self.start_device(device_id))
            layout.addWidget(start_btn)
            
        elif device.status.value == "failed":
            # 失败状态：显示智能重试按钮
            retry_options = device.get_retry_options()
            if retry_options:
                # 智能重试按钮（显示最合适的重试选项）
                main_retry_option = retry_options[0]  # 第一个是最合适的
                retry_btn = QPushButton(f"🔄 {main_retry_option['label']}")
                retry_btn.setMinimumWidth(100)  # 设置最小宽度
                retry_btn.setMaximumWidth(130)  # 增加最大宽度
                retry_btn.setMinimumHeight(38)  # 增加按钮高度
                retry_btn.setStyleSheet(self.get_button_style("retry"))
                retry_btn.clicked.connect(lambda: self.retry_device_smart(device_id, main_retry_option))
                layout.addWidget(retry_btn)

                # 更多选项按钮
                if len(retry_options) > 1:
                    more_btn = QPushButton("⋮")
                    more_btn.setMaximumWidth(30)
                    more_btn.setMinimumHeight(38)
                    more_btn.setStyleSheet(self.get_button_style("more"))
                    more_btn.clicked.connect(lambda: self.show_retry_menu(device_id, more_btn))
                    layout.addWidget(more_btn)
            else:
                # 没有重试选项时显示重置按钮
                reset_btn = QPushButton("🔄 重置")
                reset_btn.setMinimumWidth(70)   # 设置最小宽度
                reset_btn.setMaximumWidth(90)   # 增加最大宽度
                reset_btn.setMinimumHeight(38)  # 增加按钮高度
                reset_btn.setStyleSheet(self.get_button_style("reset"))
                reset_btn.clicked.connect(lambda: self.reset_device(device_id))
                layout.addWidget(reset_btn)
                
        elif device.is_active():
            # 活动状态：显示停止按钮
            stop_btn = QPushButton("⏹️ 停止")
            stop_btn.setMinimumWidth(70)   # 设置最小宽度
            stop_btn.setMaximumWidth(90)   # 增加最大宽度
            stop_btn.setMinimumHeight(38)  # 增加按钮高度
            stop_btn.setStyleSheet(self.get_button_style("stop"))
            stop_btn.clicked.connect(lambda: self.stop_device(device_id))
            layout.addWidget(stop_btn)
            
        else:
            # 其他状态：显示状态信息
            status_label = QLabel(self.get_status_text(device.status))
            status_label.setStyleSheet("color: #7f8c8d; font-size: 10px;")
            layout.addWidget(status_label)
        
        return widget
    
    def start_device(self, device_id: str):
        """开始处理设备"""
        # 这个方法将在主窗口中实现
        pass
    
    def stop_device(self, device_id: str):
        """停止处理设备"""
        # 这个方法将在主窗口中实现
        pass
    
    def retry_device_smart(self, device_id: str, retry_option: Dict[str, str]):
        """智能重试设备"""
        # 这个方法将在主窗口中实现
        pass
    
    def show_retry_menu(self, device_id: str, button: QPushButton):
        """显示重试菜单"""
        # 这个方法将在主窗口中实现
        pass
    
    def reset_device(self, device_id: str):
        """重置设备"""
        # 这个方法将在主窗口中实现
        pass
    
    @Slot()
    def on_selection_changed(self):
        """设备选择变化"""
        current_row = self.currentRow()
        if current_row >= 0:
            item = self.item(current_row, 0)
            if item:
                device_id = item.text()
                self.device_selected.emit(device_id)
    
    @Slot()
    def show_context_menu(self, position):
        """显示右键菜单"""
        item = self.itemAt(position)
        if not item:
            return
        
        row = item.row()
        device_id_item = self.item(row, 0)
        if not device_id_item:
            return
        
        device_id = device_id_item.text()
        device = self.devices.get(device_id)
        if not device:
            return
        
        menu = QMenu(self)
        
        # 根据设备状态添加不同的菜单项
        if device.status.value == "failed":
            # 失败设备的菜单
            retry_options = device.get_retry_options()
            for option in retry_options:
                action = QAction(option["label"], self)
                action.triggered.connect(lambda checked, opt=option: self.retry_device_smart(device_id, opt))
                menu.addAction(action)
            
            menu.addSeparator()
            
            # 查看错误详情
            error_action = QAction("查看错误详情", self)
            error_action.triggered.connect(lambda: self.show_error_details(device_id))
            menu.addAction(error_action)
            
        elif device.status.value in ["idle", "completed"]:
            # 空闲或完成设备的菜单
            start_action = QAction("开始处理", self)
            start_action.triggered.connect(lambda: self.start_device(device_id))
            menu.addAction(start_action)
            
        elif device.is_active():
            # 活动设备的菜单
            stop_action = QAction("停止处理", self)
            stop_action.triggered.connect(lambda: self.stop_device(device_id))
            menu.addAction(stop_action)
        
        # 通用菜单项
        menu.addSeparator()
        
        reset_action = QAction("重置设备", self)
        reset_action.triggered.connect(lambda: self.reset_device(device_id))
        menu.addAction(reset_action)
        
        remove_action = QAction("移除设备", self)
        remove_action.triggered.connect(lambda: self.remove_single_device(device_id))
        menu.addAction(remove_action)
        
        # 设备信息
        menu.addSeparator()
        info_action = QAction("设备信息", self)
        info_action.triggered.connect(lambda: self.show_device_info(device_id))
        menu.addAction(info_action)
        
        # 显示菜单
        menu.exec(self.mapToGlobal(position))
    
    def show_error_details(self, device_id: str):
        """显示错误详情"""
        # 这个方法将在主窗口中实现
        pass
    
    def remove_device_action(self, device_id: str):
        """移除设备操作"""
        # 这个方法将在主窗口中实现
        pass
    
    def remove_single_device(self, device_id: str):
        """移除单个设备"""
        # 这个方法将在主窗口中实现
        pass
    
    def show_device_info(self, device_id: str):
        """显示设备信息"""
        # 这个方法将在主窗口中实现
        pass

    def get_device_mac(self, device_id: str):
        """获取设备MAC地址"""
        # 这个方法将在主窗口中实现
        pass

class MultiDeviceUI(QMainWindow):
    """多设备管理主界面"""
    
    # 添加UI更新信号
    device_status_update_signal = Signal(DeviceInstance)
    device_log_signal = Signal(str)
    
    def __init__(self):
        super().__init__()
        
        # 设备配置
        self.default_client_type = "esp32"
        self.default_device_version = "1.0.0"
        self.idf_path = "C:\\Users\\1\\esp\\v5.4.1\\esp-idf"  # 恢复使用v5.4.1，Python环境问题已解决
        
        # 初始化设备管理器（传递idf_path参数）
        self.device_manager = MultiDeviceManager(idf_path=self.idf_path)
        self.device_manager.set_callbacks(
            device_status_callback=self.on_device_status_changed,
            progress_callback=self.on_device_progress_changed,
            log_callback=self.on_device_log
        )
        
        # 设置默认NVS直写模式
        self.device_manager.set_nvs_direct_mode(True)
        
        # UI组件
        self.device_table: Optional[DeviceTableWidget] = None
        self.log_text: Optional[QTextEdit] = None
        self.status_bar_label: Optional[QLabel] = None
        
        # 移除了编译相关变量
        
        # 初始化UI
        self.setup_ui()
        self.setup_connections()
        
        # 创建统计更新定时器
        self.stats_timer = QTimer()
        self.stats_timer.timeout.connect(self.update_statistics)
        self.stats_timer.start(2000)  # 每2秒更新一次统计信息
        
        logger.info("多设备管理界面初始化完成")
    
    def setup_ui(self):
        """设置用户界面"""
        self.setWindowTitle("小乔智能设备 - 多设备自动烧录工具")
        self.setMinimumSize(1200, 800)
        
        # 主窗口部件
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QVBoxLayout(main_widget)
        
        # 工具栏区域
        toolbar_layout = self.create_toolbar()
        main_layout.addLayout(toolbar_layout)
        
        # 主要内容区域（分割器）
        splitter = QSplitter(Qt.Vertical)
        main_layout.addWidget(splitter, 1)
        
        # 上部区域：设备列表和配置
        top_widget = self.create_top_area()
        splitter.addWidget(top_widget)
        
        # 下部区域：日志
        bottom_widget = self.create_bottom_area()
        splitter.addWidget(bottom_widget)
        
        # 设置分割器比例
        splitter.setSizes([500, 300])
        
        # 状态栏
        self.status_bar_label = QLabel("就绪")
        self.statusBar().addWidget(self.status_bar_label)
        
        # 应用样式
        self.setStyleSheet(self.get_stylesheet())
    
    def create_toolbar(self) -> QHBoxLayout:
        """创建工具栏"""
        layout = QHBoxLayout()

        # 设备管理组
        device_group = QGroupBox("设备管理")
        device_group.setMaximumHeight(80)
        device_layout = QHBoxLayout(device_group)
        device_layout.setContentsMargins(8, 8, 8, 8)

        # 自动检测设备按钮
        detect_btn = QPushButton("🔍 自动检测")
        detect_btn.setToolTip("自动检测连接的ESP32设备")
        detect_btn.clicked.connect(self.auto_detect_devices)
        device_layout.addWidget(detect_btn)

        # 添加设备按钮
        add_btn = QPushButton("➕ 手动添加")
        add_btn.setToolTip("手动添加设备端口")
        add_btn.clicked.connect(self.add_device_manually)
        device_layout.addWidget(add_btn)

        # 移除设备按钮
        remove_btn = QPushButton("➖ 移除选中")
        remove_btn.setToolTip("移除当前选中的设备")
        remove_btn.clicked.connect(self.remove_selected_device)
        device_layout.addWidget(remove_btn)

        # 移除全部设备按钮
        remove_all_btn = QPushButton("🗑️ 清空全部")
        remove_all_btn.setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; }")
        remove_all_btn.clicked.connect(self.remove_all_devices)
        remove_all_btn.setToolTip("移除所有设备（危险操作）")
        device_layout.addWidget(remove_all_btn)

        layout.addWidget(device_group)

        # 批量操作组
        batch_group = QGroupBox("批量操作")
        batch_group.setMaximumHeight(80)
        batch_layout = QHBoxLayout(batch_group)
        batch_layout.setContentsMargins(8, 8, 8, 8)

        # 批量操作按钮
        start_all_btn = QPushButton("▶️ 开始全部")
        start_all_btn.setStyleSheet("QPushButton { background-color: #27ae60; color: white; font-weight: bold; }")
        start_all_btn.setToolTip("开始处理所有设备")
        start_all_btn.clicked.connect(self.start_all_devices)
        batch_layout.addWidget(start_all_btn)

        stop_all_btn = QPushButton("⏹️ 停止全部")
        stop_all_btn.setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; }")
        stop_all_btn.setToolTip("停止所有设备处理")
        stop_all_btn.clicked.connect(self.stop_all_devices)
        batch_layout.addWidget(stop_all_btn)

        # 批量重试按钮
        retry_all_btn = QPushButton("🔄 重试失败")
        retry_all_btn.setStyleSheet("QPushButton { background-color: #f39c12; color: white; font-weight: bold; }")
        retry_all_btn.setToolTip("重试所有失败的设备")
        retry_all_btn.clicked.connect(self.retry_all_failed_devices)
        batch_layout.addWidget(retry_all_btn)

        layout.addWidget(batch_group)

        layout.addStretch()

        # 统计信息
        stats_group = QGroupBox("统计信息")
        stats_group.setMaximumHeight(80)
        stats_layout = QVBoxLayout(stats_group)
        stats_layout.setContentsMargins(8, 8, 8, 8)

        self.stats_label = QLabel("总计: 0 | 活动: 0 | 完成: 0 | 失败: 0")
        self.stats_label.setStyleSheet("font-weight: bold; color: #2c3e50;")
        stats_layout.addWidget(self.stats_label)

        layout.addWidget(stats_group)

        return layout
    
    def create_top_area(self) -> QWidget:
        """创建上部区域（设备列表和配置）"""
        widget = QWidget()
        layout = QHBoxLayout(widget)
        
        # 左侧：设备列表
        device_group = QGroupBox("设备列表")
        device_layout = QVBoxLayout(device_group)
        
        self.device_table = DeviceTableWidget()
        device_layout.addWidget(self.device_table)
        
        layout.addWidget(device_group, 3)
        
        # 右侧：配置区域
        config_widget = QWidget()
        config_main_layout = QVBoxLayout(config_widget)
        config_main_layout.setSpacing(10)

        # 基本配置组
        basic_config_group = QGroupBox("📋 基本配置")
        basic_config_layout = QFormLayout(basic_config_group)

        # 默认设备类型
        self.client_type_combo = QComboBox()
        self.client_type_combo.addItems(["esp32s3", "esp32s3-oled0.96", "esp32s3-jc", "esp32", "esp32s2", "esp32c3"])
        self.client_type_combo.setCurrentText("esp32s3")  # 设置默认选择为esp32s3
        self.client_type_combo.setToolTip("选择默认的设备类型，新添加的设备将使用此类型")
        basic_config_layout.addRow("🔧 默认设备类型:", self.client_type_combo)

        # 默认设备版本
        self.device_version_edit = QLineEdit("1.0.0")
        self.device_version_edit.setToolTip("设置默认的设备版本号")
        basic_config_layout.addRow("📦 默认设备版本:", self.device_version_edit)

        config_main_layout.addWidget(basic_config_group)
        
        # 队列状态显示区域
        queue_status_group = QGroupBox("⚡ 队列状态")
        queue_status_layout = QVBoxLayout(queue_status_group)

        # 当前处理设备
        self.current_device_label = QLabel("当前处理: 无")
        self.current_device_label.setStyleSheet("font-weight: bold; color: #27ae60;")
        queue_status_layout.addWidget(self.current_device_label)

        # 队列长度
        self.queue_size_label = QLabel("队列长度: 0")
        self.queue_size_label.setStyleSheet("color: #3498db;")
        queue_status_layout.addWidget(self.queue_size_label)

        # 队列状态
        self.queue_status_label = QLabel("队列状态: 停止")
        self.queue_status_label.setStyleSheet("color: #e74c3c;")
        queue_status_layout.addWidget(self.queue_status_label)

        config_main_layout.addWidget(queue_status_group)
        
        # 性能统计面板
        stats_group = QGroupBox("📊 性能统计")
        stats_layout = QVBoxLayout(stats_group)

        # 成功率
        self.success_rate_label = QLabel("成功率: 0%")
        self.success_rate_label.setStyleSheet("font-weight: bold; color: #27ae60;")
        stats_layout.addWidget(self.success_rate_label)

        # 平均时间
        self.avg_time_label = QLabel("平均时间: 未知")
        self.avg_time_label.setStyleSheet("color: #3498db;")
        stats_layout.addWidget(self.avg_time_label)

        # 处理数量
        self.processed_count_label = QLabel("已处理: 0")
        self.processed_count_label.setStyleSheet("color: #9b59b6;")
        stats_layout.addWidget(self.processed_count_label)

        # 统计按钮布局
        stats_btn_layout = QHBoxLayout()

        # 详细统计按钮
        self.detail_stats_btn = QPushButton("📈 详细统计")
        self.detail_stats_btn.clicked.connect(self.show_detailed_statistics)
        stats_btn_layout.addWidget(self.detail_stats_btn)

        # 清空统计按钮
        self.clear_stats_btn = QPushButton("🗑️ 清空统计")
        self.clear_stats_btn.clicked.connect(self.clear_statistics)
        stats_btn_layout.addWidget(self.clear_stats_btn)

        stats_layout.addLayout(stats_btn_layout)

        config_main_layout.addWidget(stats_group)
        
        # 烧录选项组
        flash_options_group = QGroupBox("🔥 烧录选项")
        flash_options_layout = QVBoxLayout(flash_options_group)

        # 自动开始选项
        self.auto_start_checkbox = QCheckBox("🚀 检测到设备后自动开始")
        self.auto_start_checkbox.setToolTip("启用后，检测到新设备时会自动开始处理")
        flash_options_layout.addWidget(self.auto_start_checkbox)

        # NVS直写模式选项
        self.nvs_direct_write_checkbox = QCheckBox("⚡ 启用NVS直写模式 (使用预编译固件)")
        self.nvs_direct_write_checkbox.setChecked(True)  # 默认启用
        self.nvs_direct_write_checkbox.setToolTip(
            "启用后将使用预编译的通用固件，烧录完成后直接写入NVS分区\n"
            "可以实现批量烧录多个设备，大幅提升效率"
        )
        flash_options_layout.addWidget(self.nvs_direct_write_checkbox)

        # 烧录前擦除选项
        self.erase_before_flash_checkbox = QCheckBox("🗑️ 烧录前擦除内存")
        self.erase_before_flash_checkbox.setChecked(False)  # 默认不擦除
        self.erase_before_flash_checkbox.setToolTip(
            "勾选后在烧录通用固件前会先擦除设备内存\n"
            "建议在首次烧录或遇到问题时启用"
        )
        flash_options_layout.addWidget(self.erase_before_flash_checkbox)

        # 预编译固件路径选择
        firmware_path_group = QGroupBox("📁 通用固件路径")
        firmware_path_layout = QHBoxLayout(firmware_path_group)

        self.firmware_path_edit = QLineEdit()
        self.firmware_path_edit.setPlaceholderText("选择预编译的通用固件路径...")
        firmware_path_layout.addWidget(self.firmware_path_edit)

        browse_firmware_btn = QPushButton("📂 浏览")
        browse_firmware_btn.clicked.connect(self.browse_firmware_path)
        browse_firmware_btn.setMaximumWidth(80)
        firmware_path_layout.addWidget(browse_firmware_btn)

        self.nvs_mode_widget = firmware_path_group  # 保存引用以便控制显示
        flash_options_layout.addWidget(firmware_path_group)

        config_main_layout.addWidget(flash_options_group)

        # 工具组
        tools_group = QGroupBox("🛠️ 工具")
        tools_layout = QVBoxLayout(tools_group)

        # 清理旧工作目录按钮
        cleanup_btn = QPushButton("🧹 清理旧工作目录")
        cleanup_btn.setToolTip("清理超过24小时的旧工作目录")
        cleanup_btn.clicked.connect(self.cleanup_old_workspaces)
        tools_layout.addWidget(cleanup_btn)

        config_main_layout.addWidget(tools_group)

        # 连接信号
        self.nvs_direct_write_checkbox.toggled.connect(self.on_nvs_mode_changed)
        self.erase_before_flash_checkbox.toggled.connect(self.on_erase_option_changed)

        # 初始化固件路径组件的可见性
        self.nvs_mode_widget.setVisible(self.nvs_direct_write_checkbox.isChecked())

        layout.addWidget(config_widget, 1)
        
        return widget
    
    def create_bottom_area(self) -> QWidget:
        """创建下部区域（日志）"""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # 日志标题和控制
        log_header = QHBoxLayout()
        log_header.addWidget(QLabel("运行日志"))
        
        clear_btn = QPushButton("清空日志")
        clear_btn.setMaximumWidth(80)
        clear_btn.clicked.connect(self.clear_log)
        log_header.addStretch()
        log_header.addWidget(clear_btn)
        
        layout.addLayout(log_header)
        
        # 日志文本区域
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setFont(QFont("Consolas", 9))
        layout.addWidget(self.log_text)
        
        return widget
    
    def setup_connections(self):
        """设置信号连接"""
        if self.device_table:
            self.device_table.device_selected.connect(self.on_device_selected)
            # 覆盖表格的操作方法
            self.device_table.start_device = self.start_device
            self.device_table.stop_device = self.stop_device
            self.device_table.retry_device_smart = self.retry_device_smart
            self.device_table.show_retry_menu = self.show_retry_menu
            self.device_table.reset_device = self.reset_device
            self.device_table.show_error_details = self.show_error_details
            self.device_table.remove_device_action = self.remove_selected_device
            self.device_table.remove_single_device = self.remove_single_device
            self.device_table.show_device_info = self.show_device_info
            self.device_table.get_device_mac = self.get_device_mac
        
        # 连接UI更新信号到槽函数（确保在主线程中执行）
        self.device_status_update_signal.connect(self.update_device_ui)
        self.device_log_signal.connect(self.append_log_ui)
    
    def get_stylesheet(self) -> str:
        """获取样式表"""
        return """
        QMainWindow {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #f8f9fa, stop: 1 #e9ecef);
        }

        QGroupBox {
            font-weight: bold;
            border: 2px solid #dee2e6;
            border-radius: 12px;
            margin-top: 1ex;
            padding-top: 15px;
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #ffffff, stop: 1 #f8f9fa);
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 10px 0 10px;
            color: #495057;
            font-size: 14px;
            font-weight: bold;
        }

        QPushButton {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #4dabf7, stop: 1 #339af0);
            border: none;
            color: white;
            padding: 10px 18px;
            text-align: center;
            font-size: 12px;
            margin: 3px 2px;
            border-radius: 8px;
            font-weight: bold;
            min-width: 80px;
        }

        QPushButton:hover {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #339af0, stop: 1 #228be6);
        }

        QPushButton:pressed {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #228be6, stop: 1 #1c7ed6);
        }

        QPushButton:disabled {
            background: #adb5bd;
            color: #6c757d;
        }

        QTableWidget {
            gridline-color: #dee2e6;
            background-color: white;
            alternate-background-color: #f8f9fa;
            border: 1px solid #dee2e6;
            border-radius: 8px;
            selection-background-color: #e3f2fd;
        }

        QTableWidget::item {
            padding: 10px;
            border-bottom: 1px solid #e9ecef;
        }

        QTableWidget::item:selected {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #e3f2fd, stop: 1 #bbdefb);
            color: #1565c0;
        }

        QTableWidget QHeaderView::section {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #f1f3f4, stop: 1 #e8eaed);
            padding: 8px;
            border: none;
            border-bottom: 2px solid #dee2e6;
            font-weight: bold;
            color: #495057;
        }

        QComboBox, QLineEdit, QSpinBox {
            border: 2px solid #ced4da;
            border-radius: 8px;
            padding: 8px 12px;
            font-size: 12px;
            background-color: white;
            selection-background-color: #e3f2fd;
        }

        QComboBox:focus, QLineEdit:focus, QSpinBox:focus {
            border-color: #4dabf7;
            outline: none;
        }

        QComboBox::drop-down {
            border: none;
            width: 20px;
        }

        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid #6c757d;
        }

        QTextEdit {
            border: 2px solid #ced4da;
            border-radius: 8px;
            padding: 10px;
            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
            background-color: #f8f9fa;
            selection-background-color: #e3f2fd;
        }

        QCheckBox {
            spacing: 8px;
            color: #495057;
            font-weight: 500;
        }

        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 2px solid #ced4da;
            background-color: white;
        }

        QCheckBox::indicator:checked {
            background-color: #4dabf7;
            border-color: #339af0;
            image: url(data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTIiIGhlaWdodD0iOSIgdmlld0JveD0iMCAwIDEyIDkiIGZpbGw9Im5vbmUiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+CjxwYXRoIGQ9Ik0xIDQuNUw0LjUgOEwxMSAxIiBzdHJva2U9IndoaXRlIiBzdHJva2Utd2lkdGg9IjIiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIgc3Ryb2tlLWxpbmVqb2luPSJyb3VuZCIvPgo8L3N2Zz4K);
        }

        QProgressBar {
            border: 2px solid #ced4da;
            border-radius: 8px;
            text-align: center;
            background-color: #e9ecef;
        }

        QProgressBar::chunk {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #51cf66, stop: 1 #40c057);
            border-radius: 6px;
        }

        QLabel {
            color: #495057;
        }
        """
    
    # 设备管理方法
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
                # 生成设备ID
                # 先处理端口名称中的特殊字符
                safe_port = port.replace('/', '_').replace('\\', '_')
                device_id = f"Device_{safe_port}"
                
                # 检查是否已存在
                if not self.device_manager.get_device(device_id):
                    device = self.device_manager.add_device(device_id, port)
                    device.set_device_config(
                        self.client_type_combo.currentText(),
                        "",  # 设备名称稍后自动生成
                        self.device_version_edit.text()
                    )
                    
                    self.device_table.add_device(device)
                    added_count += 1
                    
                    # 如果启用自动开始，则开始处理
                    if self.auto_start_checkbox.isChecked():
                        self.device_manager.start_device_processing(device_id)
            
            if added_count > 0:
                self.log_message(f"自动检测并添加了 {added_count} 个设备")
                QMessageBox.information(self, "检测完成", f"成功添加 {added_count} 个设备")
            else:
                QMessageBox.information(self, "检测完成", "所有设备都已存在")
                
        except Exception as e:
            logger.error(f"自动检测设备失败: {e}")
            QMessageBox.critical(self, "错误", f"自动检测设备失败: {e}")
    
    @Slot()
    def add_device_manually(self):
        """手动添加设备"""
        # 这里应该弹出一个对话框让用户输入设备信息
        # 为简化，先使用固定端口
        port, ok = QInputDialog.getText(self, "添加设备", "请输入设备端口 (如 COM3):")
        if ok and port:
            # 先处理端口名称中的特殊字符
            safe_port = port.replace('/', '_').replace('\\', '_')
            device_id = f"Device_{safe_port}"
            
            if self.device_manager.get_device(device_id):
                QMessageBox.warning(self, "警告", "该设备已存在")
                return
            
            device = self.device_manager.add_device(device_id, port)
            device.set_device_config(
                self.client_type_combo.currentText(),
                "",
                self.device_version_edit.text()
            )
            
            self.device_table.add_device(device)
            self.log_message(f"手动添加设备: {device_id} (端口: {port})")
    
    @Slot()
    def remove_selected_device(self):
        """移除选中的设备"""
        current_row = self.device_table.currentRow()
        if current_row < 0:
            QMessageBox.information(self, "提示", "请先选择一个设备")
            return
        
        item = self.device_table.item(current_row, 0)
        if not item:
            return
        
        device_id = item.text()
        
        # 确认对话框
        reply = QMessageBox.question(self, "确认", f"确定要移除设备 {device_id} 吗？",
                                   QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        
        if reply == QMessageBox.Yes:
            self.device_manager.remove_device(device_id)
            self.device_table.remove_device(device_id)
            self.log_message(f"移除设备: {device_id}")
    
    def remove_single_device(self, device_id: str):
        """移除单个设备（通过右键菜单调用）"""
        try:
            # 确认对话框
            reply = QMessageBox.question(self, "确认", f"确定要移除设备 {device_id} 吗？",
                                       QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
            
            if reply == QMessageBox.Yes:
                self.device_manager.remove_device(device_id)
                self.device_table.remove_device(device_id)
                self.log_message(f"移除设备: {device_id}")
        except Exception as e:
            logger.error(f"移除设备 {device_id} 失败: {e}")
            QMessageBox.critical(self, "错误", f"移除设备失败: {e}")
    
    @Slot()
    def remove_all_devices(self):
        """移除所有设备"""
        try:
            # 获取所有设备
            all_devices = self.device_manager.get_all_devices()
            
            if not all_devices:
                QMessageBox.information(self, "提示", "当前没有设备可以移除")
                return
            
            # 统计各种状态的设备
            active_devices = [d for d in all_devices if d.is_active()]
            completed_devices = [d for d in all_devices if d.is_completed()]
            failed_devices = [d for d in all_devices if d.is_failed()]
            idle_devices = [d for d in all_devices if d.status.value == "idle"]
            
            # 构建详细的确认消息
            confirm_message = f"确定要移除所有 {len(all_devices)} 个设备吗？\n\n"
            confirm_message += f"设备状态统计：\n"
            confirm_message += f"• 活动中设备: {len(active_devices)} 个\n"
            confirm_message += f"• 已完成设备: {len(completed_devices)} 个\n"
            confirm_message += f"• 失败设备: {len(failed_devices)} 个\n"
            confirm_message += f"• 空闲设备: {len(idle_devices)} 个\n\n"
            
            if active_devices:
                confirm_message += "⚠️ 警告：有设备正在处理中，将被强制停止！\n\n"
            
            confirm_message += "此操作不可撤销，请谨慎操作！"
            
            # 确认对话框
            reply = QMessageBox.question(
                self, "确认移除全部设备", confirm_message,
                QMessageBox.Yes | QMessageBox.No, QMessageBox.No
            )
            
            if reply == QMessageBox.Yes:
                # 首先停止所有正在处理的设备
                if active_devices:
                    self.log_message(f"正在停止 {len(active_devices)} 个活动设备...")
                    self.device_manager.stop_all_processing()
                    
                    # 等待一下让设备有时间停止
                    QApplication.processEvents()
                    import time
                    time.sleep(0.5)
                
                # 记录开始移除
                self.log_message(f"开始移除所有 {len(all_devices)} 个设备...")
                
                # 批量移除设备
                removed_count = 0
                failed_removals = []
                
                # 创建设备ID列表（避免在迭代过程中修改字典）
                device_ids = [device.device_id for device in all_devices]
                
                for device_id in device_ids:
                    try:
                        # 从设备管理器移除
                        if self.device_manager.remove_device(device_id):
                            # 从UI表格移除
                            self.device_table.remove_device(device_id)
                            removed_count += 1
                        else:
                            failed_removals.append(device_id)
                    except Exception as e:
                        logger.error(f"移除设备 {device_id} 失败: {e}")
                        failed_removals.append(device_id)
                
                # 报告结果
                if removed_count == len(all_devices):
                    self.log_message(f"成功移除所有 {removed_count} 个设备")
                    QMessageBox.information(self, "移除完成", f"成功移除所有 {removed_count} 个设备")
                else:
                    error_message = f"移除完成：成功 {removed_count} 个，失败 {len(failed_removals)} 个"
                    if failed_removals:
                        error_message += f"\n失败的设备: {', '.join(failed_removals)}"
                    
                    self.log_message(error_message)
                    QMessageBox.warning(self, "部分移除失败", error_message)
                
                # 立即更新统计信息
                self.update_statistics()
                
        except Exception as e:
            logger.error(f"移除全部设备失败: {e}")
            QMessageBox.critical(self, "错误", f"移除全部设备失败: {e}")
    
    @Slot()
    def start_all_devices(self):
        """开始处理所有设备"""
        try:
            started_count = self.device_manager.start_all_devices_processing()
            self.log_message(f"开始批量处理，共启动 {started_count} 个设备")
            
            if started_count == 0:
                QMessageBox.information(self, "提示", "没有可以开始的设备")
            
        except Exception as e:
            logger.error(f"开始全部设备失败: {e}")
            QMessageBox.critical(self, "错误", f"开始全部设备失败: {e}")
    
    @Slot()
    def stop_all_devices(self):
        """停止处理所有设备"""
        try:
            self.device_manager.stop_all_processing()
            self.log_message("已停止所有设备处理")
            
        except Exception as e:
            logger.error(f"停止全部设备失败: {e}")
            QMessageBox.critical(self, "错误", f"停止全部设备失败: {e}")
    
    @Slot()
    def retry_all_failed_devices(self):
        """重试所有失败设备"""
        try:
            # 统计失败设备数量
            failed_devices = [device for device in self.device_manager.get_all_devices() if device.is_failed()]
            
            if not failed_devices:
                QMessageBox.information(self, "提示", "没有失败的设备需要重试")
                return
            
            # 确认对话框
            reply = QMessageBox.question(
                self, "确认批量重试", 
                f"确定要重试所有 {len(failed_devices)} 个失败设备吗？\n\n"
                "这将根据各设备的失败阶段进行智能重试。",
                QMessageBox.Yes | QMessageBox.No, QMessageBox.No
            )
            
            if reply == QMessageBox.Yes:
                retried_count = self.device_manager.retry_all_failed_devices()
                self.log_message(f"批量重试完成，已重试 {retried_count} 个失败设备")
                
                if retried_count > 0:
                    QMessageBox.information(self, "重试完成", f"成功重试 {retried_count} 个设备")
                else:
                    QMessageBox.warning(self, "警告", "没有设备被重试")
                    
        except Exception as e:
            logger.error(f"批量重试失败: {e}")
            QMessageBox.critical(self, "错误", f"批量重试失败: {e}")
    
    def start_device(self, device_id: str):
        """开始处理单个设备"""
        try:
            if self.device_manager.start_device_processing(device_id):
                self.log_message(f"开始处理设备: {device_id}")
            else:
                QMessageBox.warning(self, "警告", f"无法开始处理设备 {device_id}")
                
        except Exception as e:
            logger.error(f"开始设备 {device_id} 失败: {e}")
            QMessageBox.critical(self, "错误", f"开始设备失败: {e}")
    
    def stop_device(self, device_id: str):
        """停止处理单个设备"""
        try:
            if self.device_manager.stop_device_processing(device_id):
                self.log_message(f"停止处理设备: {device_id}")
            else:
                QMessageBox.warning(self, "警告", f"无法停止处理设备 {device_id}")
                
        except Exception as e:
            logger.error(f"停止设备 {device_id} 失败: {e}")
            QMessageBox.critical(self, "错误", f"停止设备失败: {e}")
    
    def retry_device_smart(self, device_id: str, retry_option: Dict[str, str]):
        """智能重试设备"""
        try:
            action = retry_option["action"]
            phase = retry_option.get("phase")
            
            if action == "retry_current":
                success = self.device_manager.retry_device_current_phase(device_id)
            elif action == "retry_full":
                success = self.device_manager.retry_device_full(device_id)
            elif action == "retry_from_mac":
                success = self.device_manager.retry_device_from_phase(device_id, "mac_acquisition")
            elif action == "retry_from_register":
                success = self.device_manager.retry_device_from_phase(device_id, "device_registration")
            elif action == "retry_from_config":
                success = self.device_manager.retry_device_from_phase(device_id, "config_update")
            elif action == "retry_from_build":
                success = self.device_manager.retry_device_from_phase(device_id, "firmware_build")
            elif action == "skip_continue" and phase:
                success = self.device_manager.skip_phase_and_continue(device_id, phase)
            elif action == "reset":
                success = self.device_manager.reset_device(device_id)
            else:
                success = False
            
            if success:
                self.log_message(f"重试设备成功: {device_id} ({retry_option['label']})")
            else:
                QMessageBox.warning(self, "警告", f"重试设备失败: {device_id}")
                
        except Exception as e:
            logger.error(f"重试设备 {device_id} 失败: {e}")
            QMessageBox.critical(self, "错误", f"重试设备失败: {e}")
    
    def show_retry_menu(self, device_id: str, button: QPushButton):
        """显示重试菜单"""
        try:
            retry_options = self.device_manager.get_retry_options(device_id)
            if not retry_options:
                return
            
            menu = QMenu(self)
            
            for option in retry_options:
                action = QAction(option["label"], self)
                action.triggered.connect(lambda _, opt=option: self.retry_device_smart(device_id, opt))
                menu.addAction(action)
            
            # 添加分隔符和错误详情选项
            menu.addSeparator()
            error_action = QAction("查看错误详情", self)
            error_action.triggered.connect(lambda: self.show_error_details(device_id))
            menu.addAction(error_action)
            
            # 在按钮附近显示菜单
            button_rect = button.geometry()
            menu_pos = button.mapToGlobal(button_rect.bottomLeft())
            menu.exec(menu_pos)
            
        except Exception as e:
            logger.error(f"显示重试菜单失败: {e}")
    
    def reset_device(self, device_id: str):
        """重置设备"""
        try:
            reply = QMessageBox.question(
                self, "确认", f"确定要重置设备 {device_id} 吗？",
                QMessageBox.Yes | QMessageBox.No, QMessageBox.No
            )
            
            if reply == QMessageBox.Yes:
                if self.device_manager.reset_device(device_id):
                    self.log_message(f"设备已重置: {device_id}")
                else:
                    QMessageBox.warning(self, "警告", f"重置设备失败: {device_id}")
                    
        except Exception as e:
            logger.error(f"重置设备 {device_id} 失败: {e}")
            QMessageBox.critical(self, "错误", f"重置设备失败: {e}")
    
    def show_error_details(self, device_id: str):
        """显示错误详情对话框"""
        try:
            error_details = self.device_manager.get_device_error_details(device_id)
            if not error_details:
                QMessageBox.information(self, "提示", "没有错误详情")
                return
            
            dialog = ErrorDetailsDialog(device_id, error_details, self.device_manager, self)
            dialog.exec()
            
        except Exception as e:
            logger.error(f"显示错误详情失败: {e}")
            QMessageBox.critical(self, "错误", f"显示错误详情失败: {e}")
    
    def show_device_info(self, device_id: str):
        """显示设备信息对话框"""
        try:
            device = self.device_manager.get_device(device_id)
            if not device:
                QMessageBox.warning(self, "警告", f"设备 {device_id} 不存在")
                return

            dialog = DeviceInfoDialog(device, self)
            dialog.exec()

        except Exception as e:
            logger.error(f"显示设备信息失败: {e}")
            QMessageBox.critical(self, "错误", f"显示设备信息失败: {e}")

    def get_device_mac(self, device_id: str):
        """获取单个设备的MAC地址"""
        try:
            device = self.device_manager.get_device(device_id)
            if not device:
                QMessageBox.warning(self, "警告", f"设备 {device_id} 不存在")
                return

            # 检查设备是否正在处理中
            if device.is_active():
                QMessageBox.information(self, "提示", f"设备 {device_id} 正在处理中，无法单独获取MAC地址")
                return

            # 检查端口是否可用
            if not device.port:
                QMessageBox.warning(self, "警告", f"设备 {device_id} 端口信息不完整")
                return

            # 确认对话框
            reply = QMessageBox.question(
                self, "确认获取MAC",
                f"确定要获取设备 {device_id} (端口: {device.port}) 的MAC地址吗？",
                QMessageBox.Yes | QMessageBox.No, QMessageBox.Yes
            )

            if reply != QMessageBox.Yes:
                return

            # 开始获取MAC地址
            self.log_message(f"开始获取设备 {device_id} 的MAC地址...")

            # 调用设备管理器的MAC获取方法
            success = self.device_manager.get_device_mac_only(device_id)

            if success:
                # 更新设备表格显示
                if self.device_table:
                    self.device_table.update_device(device)

                QMessageBox.information(
                    self, "获取成功",
                    f"设备 {device_id} 的MAC地址获取成功！\n\n"
                    f"MAC地址: {device.mac_address}"
                )
                self.log_message(f"设备 {device_id} MAC地址获取成功: {device.mac_address}")
            else:
                QMessageBox.critical(
                    self, "获取失败",
                    f"设备 {device_id} 的MAC地址获取失败！\n\n"
                    f"错误信息: {device.error_message or '未知错误'}"
                )
                self.log_message(f"设备 {device_id} MAC地址获取失败: {device.error_message}")

        except Exception as e:
            logger.error(f"获取设备MAC地址失败: {e}")
            QMessageBox.critical(self, "错误", f"获取设备MAC地址失败: {e}")
    
    # 回调方法（在后台线程中调用，发送信号到主线程）
    def on_device_status_changed(self, device: DeviceInstance, old_status: DeviceStatus, new_status: DeviceStatus):
        """设备状态变化回调"""
        # 发送信号到主线程进行UI更新
        self.device_status_update_signal.emit(device)
        logger.debug(f"设备 {device.device_id} 状态: {old_status.value} -> {new_status.value}")
    
    def on_device_progress_changed(self, device: DeviceInstance, progress: int, message: str):
        """设备进度变化回调"""
        # 发送信号到主线程进行UI更新
        self.device_status_update_signal.emit(device)

    def on_device_log(self, device: DeviceInstance, message: str, level: int):
        """设备日志回调"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {message}"

        # 发送信号到主线程进行UI更新
        self.device_log_signal.emit(formatted_message)
    
    # UI更新槽函数（在主线程中执行）
    @Slot(DeviceInstance)
    def update_device_ui(self, device: DeviceInstance):
        """在主线程中更新设备UI"""
        if self.device_table:
            self.device_table.update_device(device)
    
    @Slot(str)
    def append_log_ui(self, formatted_message: str):
        """在主线程中添加日志"""
        if self.log_text:
            self.log_text.append(formatted_message)
            # 自动滚动到底部
            self.log_text.verticalScrollBar().setValue(
                self.log_text.verticalScrollBar().maximum()
            )
    
    # 其他事件处理
    @Slot(str)
    def on_device_selected(self, device_id: str):
        """设备选中事件"""
        device = self.device_manager.get_device(device_id)
        if device:
            # 更新状态栏显示选中设备信息
            status_text = f"已选择设备: {device_id} | 状态: {device.status.value} | 进度: {device.progress}%"
            self.status_bar_label.setText(status_text)
    
    @Slot(str)
    def on_idf_path_changed(self, path: str):
        """ESP-IDF路径变化"""
        if path and os.path.exists(path):
            self.idf_path = path
            self.device_manager.idf_path = path
            self.log_message(f"ESP-IDF路径已更新: {path}")
    
    @Slot()
    def cleanup_old_workspaces(self):
        """清理旧工作目录"""
        try:
            workspace_manager = WorkspaceManager()
            cleaned_count = workspace_manager.cleanup_old_workspaces(max_age_hours=24)
            
            self.log_message(f"清理完成，共清理 {cleaned_count} 个旧工作目录")
            QMessageBox.information(self, "清理完成", f"成功清理 {cleaned_count} 个旧工作目录")
            
        except Exception as e:
            logger.error(f"清理工作目录失败: {e}")
            QMessageBox.critical(self, "错误", f"清理工作目录失败: {e}")
    
    @Slot(bool)
    def on_nvs_mode_changed(self, enabled):
        """NVS直写模式切换"""
        try:
            # 控制固件路径选择的显示
            self.nvs_mode_widget.setVisible(enabled)

            # 更新设备管理器的模式
            if hasattr(self, 'device_manager'):
                self.device_manager.set_nvs_direct_mode(enabled)
                self.log_message(f"NVS直写模式: {'启用' if enabled else '禁用'}")

                # 如果启用且有固件路径，则设置到设备管理器
                if enabled and self.firmware_path_edit.text():
                    self.device_manager.set_universal_firmware_path(self.firmware_path_edit.text())

        except Exception as e:
            logger.error(f"切换NVS模式失败: {e}")

    @Slot(bool)
    def on_erase_option_changed(self, enabled):
        """擦除选项切换"""
        try:
            # 更新设备管理器的擦除选项
            if hasattr(self, 'device_manager'):
                self.device_manager.set_erase_before_flash(enabled)
                self.log_message(f"烧录前擦除: {'启用' if enabled else '禁用'}")

        except Exception as e:
            logger.error(f"切换擦除选项失败: {e}")
    
    @Slot()
    def browse_firmware_path(self):
        """浏览固件文件"""
        try:
            from PySide6.QtWidgets import QFileDialog
            
            file_path, _ = QFileDialog.getOpenFileName(
                self, "选择通用固件文件", "", "Bin files (*.bin);;All files (*)"
            )
            if file_path:
                self.firmware_path_edit.setText(file_path)
                if hasattr(self, 'device_manager') and self.nvs_direct_write_checkbox.isChecked():
                    self.device_manager.set_universal_firmware_path(file_path)
                    self.log_message(f"设置通用固件路径: {file_path}")
                    
        except Exception as e:
            logger.error(f"浏览固件文件失败: {e}")
            QMessageBox.critical(self, "错误", f"浏览固件文件失败: {e}")
    
    # 移除了编译通用固件方法
    
    # 移除了编译进度更新方法
    
    # 移除了编译完成和取消处理方法
    
    # 移除了查找编译固件方法
    
    # 移除了创建合并固件方法
    
    @Slot()
    def clear_log(self):
        """清空日志"""
        if self.log_text:
            self.log_text.clear()
    
    @Slot()
    def update_statistics(self):
        """更新统计信息"""
        try:
            stats = self.device_manager.get_statistics()
            stats_text = f"总计: {stats['total']} | 活动: {stats['active']} | 完成: {stats['completed']} | 失败: {stats['failed']} | 队列: {stats['queued']}"
            self.stats_label.setText(stats_text)
            
            # 更新队列状态显示
            queue_status = self.device_manager.get_queue_status()
            
            # 当前处理设备
            current_device = queue_status.get('current_processing', '无')
            self.current_device_label.setText(f"当前处理: {current_device}")
            
            # 队列长度
            queue_size = queue_status.get('queue_size', 0)
            self.queue_size_label.setText(f"队列长度: {queue_size}")
            
            # 队列状态
            queue_enabled = queue_status.get('queue_enabled', False)
            status_text = "运行中" if queue_enabled else "停止"
            self.queue_status_label.setText(f"队列状态: {status_text}")
            
            # 更新性能统计面板
            success_rate = self.device_manager.get_success_rate()
            self.success_rate_label.setText(f"成功率: {success_rate:.1f}%")
            
            avg_times = self.device_manager.get_average_processing_times()
            total_avg = avg_times.get("total")
            if total_avg:
                if total_avg < 60:
                    avg_text = f"{total_avg:.1f}s"
                else:
                    minutes = int(total_avg // 60)
                    seconds = total_avg % 60
                    avg_text = f"{minutes}m{seconds:.1f}s"
                self.avg_time_label.setText(f"平均时间: {avg_text}")
            else:
                self.avg_time_label.setText("平均时间: 未知")
            
            performance_summary = self.device_manager.get_performance_summary()
            processed_count = performance_summary.get("total_devices_processed", 0)
            self.processed_count_label.setText(f"已处理: {processed_count}")
            
        except Exception as e:
            logger.debug(f"更新统计信息失败: {e}")
    
    @Slot()
    def show_detailed_statistics(self):
        """显示详细统计信息对话框"""
        try:
            dialog = StatisticsDialog(self.device_manager, self)
            dialog.exec()
        except Exception as e:
            logger.error(f"显示详细统计失败: {e}")
            QMessageBox.critical(self, "错误", f"显示详细统计失败: {e}")
    
    @Slot()
    def clear_statistics(self):
        """清空统计数据"""
        try:
            reply = QMessageBox.question(
                self, "确认", "确定要清空所有统计数据吗？",
                QMessageBox.Yes | QMessageBox.No, QMessageBox.No
            )
            
            if reply == QMessageBox.Yes:
                self.device_manager.clear_statistics()
                self.log_message("统计数据已清空")
                # 立即更新显示
                self.update_statistics()
                
        except Exception as e:
            logger.error(f"清空统计数据失败: {e}")
            QMessageBox.critical(self, "错误", f"清空统计数据失败: {e}")
    
    def log_message(self, message: str, level: int = logging.INFO):
        """记录日志消息"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {message}"
        
        logger.log(level, message)
        
        if self.log_text:
            self.log_text.append(formatted_message)
            self.log_text.verticalScrollBar().setValue(
                self.log_text.verticalScrollBar().maximum()
            )
    
    def closeEvent(self, event):
        """窗口关闭事件"""
        try:
            self.log_message("正在关闭应用程序...")

            # 停止统计定时器
            if hasattr(self, 'stats_timer') and self.stats_timer:
                self.stats_timer.stop()

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

class StatisticsDialog(QDialog):
    """详细统计信息对话框"""
    
    def __init__(self, device_manager: MultiDeviceManager, parent=None):
        super().__init__(parent)
        self.device_manager = device_manager
        self.setWindowTitle("详细统计信息")
        self.setModal(True)
        self.resize(800, 600)
        
        self.setup_ui()
        self.load_statistics()
    
    def setup_ui(self):
        """设置界面"""
        layout = QVBoxLayout(self)
        
        # 统计摘要
        summary_group = QGroupBox("统计摘要")
        summary_layout = QFormLayout(summary_group)
        
        self.total_devices_label = QLabel("0")
        summary_layout.addRow("总处理设备数:", self.total_devices_label)
        
        self.success_devices_label = QLabel("0")
        summary_layout.addRow("成功设备数:", self.success_devices_label)
        
        self.failed_devices_label = QLabel("0")
        summary_layout.addRow("失败设备数:", self.failed_devices_label)
        
        self.success_rate_label = QLabel("0%")
        summary_layout.addRow("成功率:", self.success_rate_label)
        
        layout.addWidget(summary_group)
        
        # 平均处理时间
        avg_times_group = QGroupBox("平均处理时间")
        avg_times_layout = QFormLayout(avg_times_group)
        
        self.avg_mac_time_label = QLabel("未知")
        avg_times_layout.addRow("MAC获取:", self.avg_mac_time_label)
        
        self.avg_register_time_label = QLabel("未知")
        avg_times_layout.addRow("设备注册:", self.avg_register_time_label)
        
        self.avg_config_time_label = QLabel("未知")
        avg_times_layout.addRow("配置更新:", self.avg_config_time_label)
        
        self.avg_build_time_label = QLabel("未知")
        avg_times_layout.addRow("固件编译:", self.avg_build_time_label)
        
        self.avg_flash_time_label = QLabel("未知")
        avg_times_layout.addRow("固件烧录:", self.avg_flash_time_label)
        
        self.avg_total_time_label = QLabel("未知")
        avg_times_layout.addRow("总处理时间:", self.avg_total_time_label)
        
        layout.addWidget(avg_times_group)
        
        # 最快/最慢设备
        extremes_group = QGroupBox("性能记录")
        extremes_layout = QFormLayout(extremes_group)
        
        self.fastest_device_label = QLabel("未知")
        extremes_layout.addRow("最快设备:", self.fastest_device_label)
        
        self.slowest_device_label = QLabel("未知")
        extremes_layout.addRow("最慢设备:", self.slowest_device_label)
        
        layout.addWidget(extremes_group)
        
        # 最近完成设备列表
        recent_group = QGroupBox("最近完成设备 (最多10个)")
        recent_layout = QVBoxLayout(recent_group)
        
        self.recent_table = QTableWidget()
        self.recent_table.setColumnCount(5)
        self.recent_table.setHorizontalHeaderLabels(["设备ID", "状态", "完成时间", "总耗时", "MAC地址"])
        self.recent_table.horizontalHeader().setStretchLastSection(True)
        recent_layout.addWidget(self.recent_table)
        
        layout.addWidget(recent_group)
        
        # 按钮
        button_layout = QHBoxLayout()
        
        refresh_btn = QPushButton("刷新")
        refresh_btn.clicked.connect(self.load_statistics)
        button_layout.addWidget(refresh_btn)
        
        export_btn = QPushButton("导出统计")
        export_btn.clicked.connect(self.export_statistics)
        button_layout.addWidget(export_btn)
        
        button_layout.addStretch()
        
        close_btn = QPushButton("关闭")
        close_btn.clicked.connect(self.accept)
        button_layout.addWidget(close_btn)
        
        layout.addLayout(button_layout)
    
    def load_statistics(self):
        """加载统计数据"""
        try:
            # 获取性能摘要
            summary = self.device_manager.get_performance_summary()
            
            # 更新摘要信息
            self.total_devices_label.setText(str(summary.get("total_devices_processed", 0)))
            self.success_devices_label.setText(str(summary.get("successful_devices", 0)))
            self.failed_devices_label.setText(str(summary.get("failed_devices", 0)))
            self.success_rate_label.setText(f"{summary.get('success_rate', 0):.1f}%")
            
            # 更新平均时间
            avg_times = summary.get("average_times", {})
            
            from device_instance import ProcessingPhase
            
            def format_time(seconds):
                if seconds is None:
                    return "未知"
                elif seconds < 1:
                    return f"{seconds * 1000:.0f}ms"
                elif seconds < 60:
                    return f"{seconds:.1f}s"
                else:
                    minutes = int(seconds // 60)
                    secs = seconds % 60
                    return f"{minutes}m{secs:.1f}s"
            
            self.avg_mac_time_label.setText(format_time(avg_times.get(ProcessingPhase.MAC_ACQUISITION)))
            self.avg_register_time_label.setText(format_time(avg_times.get(ProcessingPhase.DEVICE_REGISTRATION)))
            self.avg_config_time_label.setText(format_time(avg_times.get(ProcessingPhase.CONFIG_UPDATE)))
            self.avg_build_time_label.setText(format_time(avg_times.get(ProcessingPhase.FIRMWARE_BUILD)))
            self.avg_flash_time_label.setText(format_time(avg_times.get(ProcessingPhase.FIRMWARE_FLASH)))
            self.avg_total_time_label.setText(format_time(avg_times.get("total")))
            
            # 更新最快/最慢设备
            fastest = summary.get("fastest_device")
            slowest = summary.get("slowest_device")
            
            if fastest:
                self.fastest_device_label.setText(f"{fastest['device_id']} ({format_time(fastest['duration'])})")
            else:
                self.fastest_device_label.setText("未知")
                
            if slowest:
                self.slowest_device_label.setText(f"{slowest['device_id']} ({format_time(slowest['duration'])})")
            else:
                self.slowest_device_label.setText("未知")
            
            # 更新最近完成设备表格
            recent_completions = self.device_manager.statistics.get_recent_completions(10)
            self.recent_table.setRowCount(len(recent_completions))
            
            for row, completion in enumerate(recent_completions):
                self.recent_table.setItem(row, 0, QTableWidgetItem(completion["device_id"]))
                self.recent_table.setItem(row, 1, QTableWidgetItem("成功" if completion["success"] else "失败"))
                
                completed_at = completion["completed_at"]
                if isinstance(completed_at, str):
                    completed_time = completed_at
                else:
                    completed_time = completed_at.strftime("%Y-%m-%d %H:%M:%S")
                self.recent_table.setItem(row, 2, QTableWidgetItem(completed_time))
                
                total_duration = completion["timing_stats"].get("total_duration")
                self.recent_table.setItem(row, 3, QTableWidgetItem(format_time(total_duration)))
                self.recent_table.setItem(row, 4, QTableWidgetItem(completion.get("mac_address", "未知")))
            
        except Exception as e:
            logger.error(f"加载统计数据失败: {e}")
            # 显示错误信息
            self.total_devices_label.setText("加载失败")
    
    def export_statistics(self):
        """导出统计数据"""
        try:
            from PySide6.QtWidgets import QFileDialog
            import json
            
            filename, _ = QFileDialog.getSaveFileName(
                self, "导出统计数据", "statistics.json", "JSON files (*.json)"
            )
            
            if filename:
                stats_data = self.device_manager.get_timing_statistics()
                with open(filename, 'w', encoding='utf-8') as f:
                    json.dump(stats_data, f, ensure_ascii=False, indent=2, default=str)
                
                from PySide6.QtWidgets import QMessageBox
                QMessageBox.information(self, "导出成功", f"统计数据已导出到: {filename}")
                
        except Exception as e:
            logger.error(f"导出统计数据失败: {e}")
            from PySide6.QtWidgets import QMessageBox
            QMessageBox.critical(self, "导出失败", f"导出统计数据失败: {e}")

class ErrorDetailsDialog(QDialog):
    """错误详情对话框"""
    
    def __init__(self, device_id: str, error_details: Dict[str, Any], device_manager, parent=None):
        super().__init__(parent)
        self.device_id = device_id
        self.error_details = error_details
        self.device_manager = device_manager
        
        self.setWindowTitle(f"错误详情 - {device_id}")
        self.setModal(True)
        self.resize(600, 500)
        
        self.setup_ui()
        self.load_error_details()
    
    def setup_ui(self):
        """设置界面"""
        layout = QVBoxLayout(self)
        
        # 基本错误信息
        basic_group = QGroupBox("基本错误信息")
        basic_layout = QFormLayout(basic_group)
        
        self.error_message_label = QLabel()
        basic_layout.addRow("错误消息:", self.error_message_label)
        
        self.failed_phase_label = QLabel()
        basic_layout.addRow("失败阶段:", self.failed_phase_label)
        
        self.retry_count_label = QLabel()
        basic_layout.addRow("重试次数:", self.retry_count_label)
        
        self.error_time_label = QLabel()
        basic_layout.addRow("错误时间:", self.error_time_label)
        
        layout.addWidget(basic_group)
        
        # 详细错误信息
        details_group = QGroupBox("详细错误信息")
        details_layout = QVBoxLayout(details_group)
        
        self.details_text = QTextEdit()
        self.details_text.setReadOnly(True)
        self.details_text.setFont(QFont("Consolas", 9))
        details_layout.addWidget(self.details_text)
        
        layout.addWidget(details_group)
        
        # 重试选项
        retry_group = QGroupBox("重试选项")
        retry_layout = QVBoxLayout(retry_group)
        
        self.retry_buttons_layout = QHBoxLayout()
        retry_layout.addLayout(self.retry_buttons_layout)
        
        layout.addWidget(retry_group)
        
        # 按钮
        button_layout = QHBoxLayout()
        
        export_btn = QPushButton("导出错误日志")
        export_btn.clicked.connect(self.export_error_log)
        button_layout.addWidget(export_btn)
        
        button_layout.addStretch()
        
        close_btn = QPushButton("关闭")
        close_btn.clicked.connect(self.accept)
        button_layout.addWidget(close_btn)
        
        layout.addLayout(button_layout)
    
    def load_error_details(self):
        """加载错误详情"""
        try:
            # 基本信息
            self.error_message_label.setText(self.error_details.get("error_message", "未知错误"))
            
            failed_phase = self.error_details.get("failed_phase", "未知")
            phase_names = {
                "mac_acquisition": "MAC地址获取",
                "device_registration": "设备注册", 
                "config_update": "配置更新",
                "firmware_build": "固件编译",
                "firmware_flash": "固件烧录"
            }
            self.failed_phase_label.setText(phase_names.get(failed_phase, failed_phase))
            
            self.retry_count_label.setText(str(self.error_details.get("retry_count", 0)))
            
            # 详细错误信息
            last_error = self.error_details.get("last_error_details", {})
            if last_error:
                error_time = last_error.get("error_time", "未知时间")
                self.error_time_label.setText(error_time)
                
                details_text = f"设备ID: {self.device_id}\n"
                details_text += f"错误消息: {last_error.get('error_message', '未知')}\n"
                details_text += f"失败阶段: {last_error.get('failed_phase', '未知')}\n"
                details_text += f"错误时间: {error_time}\n"
                details_text += f"重试次数: {last_error.get('retry_count', 0)}\n"
                details_text += f"当前状态: {last_error.get('current_status', '未知')}\n"
                
                self.details_text.setPlainText(details_text)
            
            # 重试选项按钮
            retry_options = self.error_details.get("retry_options", [])
            for option in retry_options:
                btn = QPushButton(option["label"])
                btn.clicked.connect(lambda _, opt=option: self.perform_retry(opt))
                if option["action"] == "retry_current":
                    btn.setStyleSheet("QPushButton { background-color: #e67e22; color: white; font-weight: bold; }")
                elif option["action"] == "retry_full":
                    btn.setStyleSheet("QPushButton { background-color: #3498db; color: white; }")
                elif option["action"] == "reset":
                    btn.setStyleSheet("QPushButton { background-color: #95a5a6; color: white; }")
                    
                self.retry_buttons_layout.addWidget(btn)
                
        except Exception as e:
            logger.error(f"加载错误详情失败: {e}")
            self.error_message_label.setText("加载错误详情失败")
    
    def perform_retry(self, retry_option: Dict[str, str]):
        """执行重试操作"""
        try:
            action = retry_option["action"]
            phase = retry_option.get("phase")
            
            if action == "retry_current":
                success = self.device_manager.retry_device_current_phase(self.device_id)
            elif action == "retry_full":
                success = self.device_manager.retry_device_full(self.device_id)
            elif action.startswith("retry_from"):
                success = self.device_manager.retry_device_from_phase(self.device_id, phase)
            elif action == "skip_continue" and phase:
                success = self.device_manager.skip_phase_and_continue(self.device_id, phase)
            elif action == "reset":
                success = self.device_manager.reset_device(self.device_id)
            else:
                success = False
            
            if success:
                QMessageBox.information(self, "成功", f"重试操作已启动: {retry_option['label']}")
                self.accept()  # 关闭对话框
            else:
                QMessageBox.warning(self, "失败", f"重试操作失败: {retry_option['label']}")
                
        except Exception as e:
            logger.error(f"执行重试操作失败: {e}")
            QMessageBox.critical(self, "错误", f"执行重试操作失败: {e}")
    
    def export_error_log(self):
        """导出错误日志"""
        try:
            from PySide6.QtWidgets import QFileDialog
            import json
            
            filename, _ = QFileDialog.getSaveFileName(
                self, "导出错误日志", f"error_log_{self.device_id}.json", "JSON files (*.json)"
            )
            
            if filename:
                export_data = {
                    "device_id": self.device_id,
                    "export_time": datetime.now().isoformat(),
                    "error_details": self.error_details
                }
                
                with open(filename, 'w', encoding='utf-8') as f:
                    json.dump(export_data, f, ensure_ascii=False, indent=2)
                
                QMessageBox.information(self, "导出成功", f"错误日志已导出到: {filename}")
                
        except Exception as e:
            logger.error(f"导出错误日志失败: {e}")
            QMessageBox.critical(self, "导出失败", f"导出错误日志失败: {e}")

class DeviceInfoDialog(QDialog):
    """设备信息对话框"""
    
    def __init__(self, device: DeviceInstance, parent=None):
        super().__init__(parent)
        self.device = device
        
        self.setWindowTitle(f"设备信息 - {device.device_id}")
        self.setModal(True)
        self.resize(500, 400)
        
        self.setup_ui()
        self.load_device_info()
    
    def setup_ui(self):
        """设置界面"""
        layout = QVBoxLayout(self)
        
        # 基本信息
        basic_group = QGroupBox("基本信息")
        basic_layout = QFormLayout(basic_group)
        
        self.device_id_label = QLabel()
        basic_layout.addRow("设备ID:", self.device_id_label)
        
        self.port_label = QLabel()
        basic_layout.addRow("端口:", self.port_label)
        
        self.mac_label = QLabel()
        basic_layout.addRow("MAC地址:", self.mac_label)
        
        self.client_id_label = QLabel()
        basic_layout.addRow("Client ID:", self.client_id_label)
        
        self.bind_key_label = QLabel()
        basic_layout.addRow("绑定码:", self.bind_key_label)
        
        layout.addWidget(basic_group)
        
        # 配置信息
        config_group = QGroupBox("设备配置")
        config_layout = QFormLayout(config_group)
        
        self.client_type_label = QLabel()
        config_layout.addRow("设备类型:", self.client_type_label)
        
        self.device_name_label = QLabel()
        config_layout.addRow("设备名称:", self.device_name_label)
        
        self.device_version_label = QLabel()
        config_layout.addRow("设备版本:", self.device_version_label)
        
        layout.addWidget(config_group)
        
        # 状态信息
        status_group = QGroupBox("状态信息")
        status_layout = QFormLayout(status_group)
        
        self.status_label = QLabel()
        status_layout.addRow("当前状态:", self.status_label)
        
        self.progress_label = QLabel()
        status_layout.addRow("进度:", self.progress_label)
        
        self.retry_count_label = QLabel()
        status_layout.addRow("重试次数:", self.retry_count_label)
        
        self.created_time_label = QLabel()
        status_layout.addRow("创建时间:", self.created_time_label)
        
        self.started_time_label = QLabel()
        status_layout.addRow("开始时间:", self.started_time_label)
        
        self.completed_time_label = QLabel()
        status_layout.addRow("完成时间:", self.completed_time_label)
        
        layout.addWidget(status_group)
        
        # 按钮
        button_layout = QHBoxLayout()
        
        refresh_btn = QPushButton("刷新")
        refresh_btn.clicked.connect(self.load_device_info)
        button_layout.addWidget(refresh_btn)
        
        button_layout.addStretch()
        
        close_btn = QPushButton("关闭")
        close_btn.clicked.connect(self.accept)
        button_layout.addWidget(close_btn)
        
        layout.addLayout(button_layout)
    
    def load_device_info(self):
        """加载设备信息"""
        try:
            # 基本信息
            self.device_id_label.setText(self.device.device_id)
            self.port_label.setText(self.device.port or "未设置")
            self.mac_label.setText(self.device.mac_address or "未获取")
            self.client_id_label.setText(self.device.client_id or "未注册")
            self.bind_key_label.setText(self.device.bind_key or "无")
            
            # 配置信息
            self.client_type_label.setText(self.device.client_type)
            self.device_name_label.setText(self.device.device_name or "未设置")
            self.device_version_label.setText(self.device.device_version)
            
            # 状态信息
            status_map = {
                "idle": "空闲",
                "detecting": "检测中",
                "mac_getting": "获取MAC",
                "registering": "注册中",
                "config_updating": "更新配置",
                "building": "编译中",
                "flashing": "烧录中",
                "completed": "完成",
                "failed": "失败",
                "cancelled": "已取消"
            }
            status_text = status_map.get(self.device.status.value, self.device.status.value)
            if self.device.is_failed() and self.device.error_message:
                status_text += f" ({self.device.error_message})"
            self.status_label.setText(status_text)
            
            progress_text = f"{self.device.progress}%"
            if self.device.progress_message:
                progress_text += f" - {self.device.progress_message}"
            self.progress_label.setText(progress_text)
            
            self.retry_count_label.setText(str(getattr(self.device, 'retry_count', 0)))
            
            self.created_time_label.setText(self.device.created_at.strftime("%Y-%m-%d %H:%M:%S"))
            
            if self.device.started_at:
                self.started_time_label.setText(self.device.started_at.strftime("%Y-%m-%d %H:%M:%S"))
            else:
                self.started_time_label.setText("未开始")
            
            if self.device.completed_at:
                self.completed_time_label.setText(self.device.completed_at.strftime("%Y-%m-%d %H:%M:%S"))
            else:
                self.completed_time_label.setText("未完成")
                
        except Exception as e:
            logger.error(f"加载设备信息失败: {e}")
            QMessageBox.critical(self, "错误", f"加载设备信息失败: {e}")

def run_multi_device_ui():
    """运行多设备管理界面"""
    if not HAS_PYSIDE6:
        print("错误: 需要安装PySide6库")
        return 1
    
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    
    # 创建并显示主窗口
    window = MultiDeviceUI()
    window.showMaximized()  # 默认全屏启动
    
    return app.exec()

if __name__ == "__main__":
    # 需要额外导入QInputDialog
    from PySide6.QtWidgets import QInputDialog
    
    # 设置日志级别
    logging.basicConfig(level=logging.INFO)
    
    # 运行应用
    sys.exit(run_multi_device_ui()) 