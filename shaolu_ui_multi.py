#!/usr/bin/env python3
import os
import sys
import logging
from datetime import datetime
from typing import Dict, List, Optional

# 检查PySide6依赖
try:
    from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                                QPushButton, QLabel, QTableWidget, QTableWidgetItem, QHeaderView,
                                QProgressBar, QTextEdit, QComboBox, QLineEdit, QGroupBox, QFormLayout,
                                QSplitter, QFrame, QMessageBox, QTabWidget, QCheckBox, QSpinBox,
                                QAbstractItemView, QMenu, QInputDialog)
    from PySide6.QtCore import Qt, QThread, Signal, Slot, QTimer
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
        """设置表格"""
        # 设置列
        columns = ["设备ID", "端口", "状态", "进度", "MAC地址", "Client ID", "操作"]
        self.setColumnCount(len(columns))
        self.setHorizontalHeaderLabels(columns)
        
        # 设置表格属性
        self.setAlternatingRowColors(True)
        self.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.setSelectionMode(QAbstractItemView.SingleSelection)
        self.verticalHeader().setVisible(False)
        
        # 调整列宽
        header = self.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.Fixed)  # 设备ID
        header.setSectionResizeMode(1, QHeaderView.Fixed)  # 端口
        header.setSectionResizeMode(2, QHeaderView.Fixed)  # 状态
        header.setSectionResizeMode(3, QHeaderView.Stretch)  # 进度
        header.setSectionResizeMode(4, QHeaderView.Fixed)  # MAC地址
        header.setSectionResizeMode(5, QHeaderView.Fixed)  # Client ID
        header.setSectionResizeMode(6, QHeaderView.Fixed)  # 操作
        
        self.setColumnWidth(0, 100)  # 设备ID
        self.setColumnWidth(1, 80)   # 端口
        self.setColumnWidth(2, 80)   # 状态
        self.setColumnWidth(4, 120)  # MAC地址
        self.setColumnWidth(5, 120)  # Client ID
        self.setColumnWidth(6, 100)  # 操作
        
        # 连接信号
        self.itemSelectionChanged.connect(self.on_selection_changed)
    
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
        
        # 状态
        status_item = QTableWidgetItem(self.get_status_text(device.status))
        status_item.setData(Qt.UserRole, device_id)
        self.setItem(row, 2, status_item)
        
        # 进度条
        progress_bar = QProgressBar()
        progress_bar.setMinimum(0)
        progress_bar.setMaximum(100)
        progress_bar.setValue(device.progress)
        progress_bar.setFormat(f"{device.progress}% - {device.progress_message}")
        self.setCellWidget(row, 3, progress_bar)
        
        # MAC地址
        self.setItem(row, 4, QTableWidgetItem(device.mac_address or "未获取"))
        
        # Client ID
        self.setItem(row, 5, QTableWidgetItem(device.client_id or "未获取"))
        
        # 操作按钮
        action_widget = self.create_action_widget(device_id)
        self.setCellWidget(row, 6, action_widget)
        
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
        
        # 更新状态
        status_item = self.item(row, 2)
        if status_item:
            status_item.setText(self.get_status_text(device.status))
            # 设置状态颜色
            color = self.get_status_color(device.status)
            status_item.setBackground(color)
        
        # 更新进度
        progress_bar = self.cellWidget(row, 3)
        if isinstance(progress_bar, QProgressBar):
            progress_bar.setValue(device.progress)
            progress_bar.setFormat(f"{device.progress}% - {device.progress_message}")
        
        # 更新MAC地址
        mac_item = self.item(row, 4)
        if mac_item:
            mac_item.setText(device.mac_address or "未获取")
        
        # 更新Client ID
        client_item = self.item(row, 5)
        if client_item:
            client_item.setText(device.client_id or "未获取")
    
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
    
    def create_action_widget(self, device_id: str) -> QWidget:
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
    
    def start_device(self, device_id: str):
        """开始处理设备"""
        # 这个方法将在主窗口中实现
        pass
    
    def stop_device(self, device_id: str):
        """停止处理设备"""
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

class MultiDeviceUI(QMainWindow):
    """多设备管理主界面"""
    
    def __init__(self):
        super().__init__()
        
        # 初始化设备管理器
        self.device_manager = MultiDeviceManager(max_workers=4)
        self.device_manager.set_callbacks(
            device_status_callback=self.on_device_status_changed,
            progress_callback=self.on_device_progress_changed,
            log_callback=self.on_device_log
        )
        
        # UI组件
        self.device_table: Optional[DeviceTableWidget] = None
        self.log_text: Optional[QTextEdit] = None
        self.status_bar_label: Optional[QLabel] = None
        
        # 设备配置
        self.default_client_type = "esp32"
        self.default_device_version = "1.0.0"
        self.idf_path = "C:\\Users\\1\\esp\\v5.4.1\\esp-idf"  # 默认ESP-IDF路径
        
        # 初始化UI
        self.setup_ui()
        self.setup_connections()
        
        # 定时器用于更新统计信息
        self.stats_timer = QTimer()
        self.stats_timer.timeout.connect(self.update_statistics)
        self.stats_timer.start(1000)  # 每秒更新一次
        
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
        
        # 自动检测设备按钮
        detect_btn = QPushButton("自动检测设备")
        detect_btn.clicked.connect(self.auto_detect_devices)
        layout.addWidget(detect_btn)
        
        # 添加设备按钮
        add_btn = QPushButton("手动添加设备")
        add_btn.clicked.connect(self.add_device_manually)
        layout.addWidget(add_btn)
        
        # 移除设备按钮
        remove_btn = QPushButton("移除设备")
        remove_btn.clicked.connect(self.remove_selected_device)
        layout.addWidget(remove_btn)
        
        layout.addSpacing(20)
        
        # 批量操作按钮
        start_all_btn = QPushButton("开始全部")
        start_all_btn.setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; }")
        start_all_btn.clicked.connect(self.start_all_devices)
        layout.addWidget(start_all_btn)
        
        stop_all_btn = QPushButton("停止全部")
        stop_all_btn.clicked.connect(self.stop_all_devices)
        layout.addWidget(stop_all_btn)
        
        layout.addSpacing(20)
        
        # ESP-IDF路径选择
        layout.addWidget(QLabel("ESP-IDF路径:"))
        self.idf_path_combo = QComboBox()
        self.idf_path_combo.setEditable(True)
        self.idf_path_combo.addItem("C:\\Users\\1\\esp\\v5.4.1\\esp-idf")
        self.idf_path_combo.addItem("C:\\Users\\1\\esp\\v5.3.2\\esp-idf")
        self.idf_path_combo.currentTextChanged.connect(self.on_idf_path_changed)
        layout.addWidget(self.idf_path_combo)
        
        layout.addStretch()
        
        # 统计信息
        self.stats_label = QLabel("总计: 0 | 活动: 0 | 完成: 0 | 失败: 0")
        layout.addWidget(self.stats_label)
        
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
        config_group = QGroupBox("设备配置")
        config_layout = QFormLayout(config_group)
        
        # 默认设备类型
        self.client_type_combo = QComboBox()
        self.client_type_combo.addItems(["esp32", "esp32s2", "esp32s3", "esp32c3"])
        config_layout.addRow("默认设备类型:", self.client_type_combo)
        
        # 默认设备版本
        self.device_version_edit = QLineEdit("1.0.0")
        config_layout.addRow("默认设备版本:", self.device_version_edit)
        
        # 并发数设置
        self.max_workers_spin = QSpinBox()
        self.max_workers_spin.setMinimum(1)
        self.max_workers_spin.setMaximum(10)
        self.max_workers_spin.setValue(4)
        self.max_workers_spin.valueChanged.connect(self.on_max_workers_changed)
        config_layout.addRow("最大并发数:", self.max_workers_spin)
        
        # 自动开始选项
        self.auto_start_checkbox = QCheckBox("检测到设备后自动开始")
        config_layout.addRow(self.auto_start_checkbox)
        
        # 清理旧工作目录按钮
        cleanup_btn = QPushButton("清理旧工作目录")
        cleanup_btn.clicked.connect(self.cleanup_old_workspaces)
        config_layout.addRow(cleanup_btn)
        
        layout.addWidget(config_group, 1)
        
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
    
    def get_stylesheet(self) -> str:
        """获取样式表"""
        return """
        QMainWindow {
            background-color: #f5f5f5;
        }
        
        QGroupBox {
            font-weight: bold;
            border: 2px solid #cccccc;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 10px;
            background-color: white;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 8px 0 8px;
            color: #2c3e50;
            font-size: 14px;
        }
        
        QPushButton {
            background-color: #3498db;
            border: none;
            color: white;
            padding: 8px 16px;
            text-align: center;
            font-size: 12px;
            margin: 2px 1px;
            border-radius: 6px;
            font-weight: bold;
        }
        
        QPushButton:hover {
            background-color: #2980b9;
        }
        
        QPushButton:pressed {
            background-color: #21618c;
        }
        
        QPushButton:disabled {
            background-color: #bdc3c7;
            color: #7f8c8d;
        }
        
        QTableWidget {
            gridline-color: #e0e0e0;
            background-color: white;
            alternate-background-color: #f9f9f9;
        }
        
        QTableWidget::item {
            padding: 8px;
        }
        
        QTableWidget::item:selected {
            background-color: #3498db;
            color: white;
        }
        
        QComboBox, QLineEdit, QSpinBox {
            border: 2px solid #bdc3c7;
            border-radius: 6px;
            padding: 6px;
            font-size: 12px;
            background-color: white;
        }
        
        QTextEdit {
            border: 2px solid #bdc3c7;
            border-radius: 6px;
            padding: 8px;
            font-family: 'Consolas', 'Monaco', monospace;
            background-color: #f8f9fa;
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
    
    # 回调方法
    def on_device_status_changed(self, device: DeviceInstance, old_status: DeviceStatus, new_status: DeviceStatus):
        """设备状态变化回调"""
        self.device_table.update_device(device)
        logger.debug(f"设备 {device.device_id} 状态: {old_status.value} -> {new_status.value}")
    
    def on_device_progress_changed(self, device: DeviceInstance, progress: int, message: str):
        """设备进度变化回调"""
        self.device_table.update_device(device)
    
    def on_device_log(self, device: DeviceInstance, message: str, level: int):
        """设备日志回调"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {message}"
        
        # 添加到日志文本
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
    
    @Slot(int)
    def on_max_workers_changed(self, value: int):
        """并发数变化"""
        # 这里需要重新创建device_manager，或者动态调整线程池大小
        self.log_message(f"最大并发数已设置为: {value}")
    
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
            stats_text = f"总计: {stats['total']} | 活动: {stats['active']} | 完成: {stats['completed']} | 失败: {stats['failed']}"
            self.stats_label.setText(stats_text)
            
        except Exception as e:
            logger.debug(f"更新统计信息失败: {e}")
    
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
    window = MultiDeviceUI()
    window.show()
    
    return app.exec()

if __name__ == "__main__":
    # 需要额外导入QInputDialog
    from PySide6.QtWidgets import QInputDialog
    
    # 设置日志级别
    logging.basicConfig(level=logging.INFO)
    
    # 运行应用
    sys.exit(run_multi_device_ui()) 