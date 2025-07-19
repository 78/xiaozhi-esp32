#!/usr/bin/env python3
import sys
import logging
from datetime import datetime
from typing import Optional, Dict, Any, List
from PySide6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QTableWidget, 
                              QTableWidgetItem, QPushButton, QLineEdit, QLabel,
                              QMessageBox, QHeaderView, QAbstractItemView, QMenu,
                              QInputDialog, QFormLayout, QComboBox, QTextEdit,
                              QTabWidget, QWidget, QSplitter, QGroupBox)
from PySide6.QtCore import Qt, Signal, QTimer
from PySide6.QtGui import QFont, QColor, QAction
from user_manager import get_user_manager
from auth import get_auth_manager
from flash_logger import get_flash_logger

logger = logging.getLogger("xiaozhi.user_management")

class UserManagementDialog(QDialog):
    """用户管理对话框"""
    
    def __init__(self, current_user_info: Dict[str, Any], parent=None):
        super().__init__(parent)
        self.current_user_info = current_user_info
        self.user_manager = get_user_manager()
        self.auth_manager = get_auth_manager()
        self.flash_logger = get_flash_logger()
        
        self.setup_ui()
        self.setup_connections()
        self.load_data()
        
        # 设置窗口属性
        self.setWindowTitle("用户管理")
        self.setMinimumSize(1200, 800)
        self.resize(1200, 800)
        self.setModal(True)
        
        logger.info("用户管理对话框初始化完成")
    
    def setup_ui(self):
        """设置用户界面"""
        layout = QVBoxLayout(self)
        
        # 创建标签页
        self.tab_widget = QTabWidget()
        
        # 用户管理标签页
        self.user_tab = self.create_user_tab()
        self.tab_widget.addTab(self.user_tab, "用户管理")
        
        # 在线用户标签页
        self.session_tab = self.create_session_tab()
        self.tab_widget.addTab(self.session_tab, "在线用户")
        
        # 烧录记录标签页
        self.record_tab = self.create_record_tab()
        self.tab_widget.addTab(self.record_tab, "烧录记录")
        
        # 统计信息标签页
        self.stats_tab = self.create_stats_tab()
        self.tab_widget.addTab(self.stats_tab, "统计信息")
        
        layout.addWidget(self.tab_widget)
        
        # 底部按钮
        button_layout = QHBoxLayout()
        button_layout.setContentsMargins(10, 10, 10, 10)
        button_layout.setSpacing(10)
        button_layout.addStretch()

        self.refresh_button = QPushButton("刷新")
        self.refresh_button.setMinimumSize(80, 35)
        self.refresh_button.setStyleSheet("""
            QPushButton {
                background-color: #3498db;
                color: white;
                border: none;
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #2980b9;
            }
        """)
        self.refresh_button.clicked.connect(self.load_data)
        button_layout.addWidget(self.refresh_button)

        self.close_button = QPushButton("关闭")
        self.close_button.setMinimumSize(80, 35)
        self.close_button.setStyleSheet("""
            QPushButton {
                background-color: #95a5a6;
                color: white;
                border: none;
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #7f8c8d;
            }
        """)
        self.close_button.clicked.connect(self.accept)
        button_layout.addWidget(self.close_button)

        layout.addLayout(button_layout)
        
        # 应用样式
        self.setStyleSheet(self.get_dialog_style())
    
    def create_user_tab(self) -> QWidget:
        """创建用户管理标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # 工具栏
        toolbar_layout = QHBoxLayout()
        
        # 搜索框
        self.search_edit = QLineEdit()
        self.search_edit.setPlaceholderText("搜索用户名、邮箱或姓名...")
        self.search_edit.setMaximumWidth(300)
        toolbar_layout.addWidget(QLabel("搜索:"))
        toolbar_layout.addWidget(self.search_edit)
        
        toolbar_layout.addStretch()
        
        # 添加用户按钮
        self.add_user_button = QPushButton("添加用户")
        self.add_user_button.clicked.connect(self.add_user)
        toolbar_layout.addWidget(self.add_user_button)
        
        layout.addLayout(toolbar_layout)
        
        # 用户表格
        self.user_table = QTableWidget()
        self.setup_user_table()
        layout.addWidget(self.user_table)
        
        return widget
    
    def setup_user_table(self):
        """设置用户表格"""
        columns = ["ID", "用户名", "邮箱", "姓名", "状态", "创建时间", "最后登录", "烧录次数", "成功率"]
        self.user_table.setColumnCount(len(columns))
        self.user_table.setHorizontalHeaderLabels(columns)
        
        # 设置表格属性
        self.user_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.user_table.setAlternatingRowColors(True)
        self.user_table.verticalHeader().setVisible(False)
        self.user_table.verticalHeader().setDefaultSectionSize(35)
        
        # 设置列宽
        header = self.user_table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.Fixed)  # ID
        header.setSectionResizeMode(1, QHeaderView.Fixed)  # 用户名
        header.setSectionResizeMode(2, QHeaderView.Stretch)  # 邮箱
        header.setSectionResizeMode(3, QHeaderView.Fixed)  # 姓名
        header.setSectionResizeMode(4, QHeaderView.Fixed)  # 状态
        header.setSectionResizeMode(5, QHeaderView.Fixed)  # 创建时间
        header.setSectionResizeMode(6, QHeaderView.Fixed)  # 最后登录
        header.setSectionResizeMode(7, QHeaderView.Fixed)  # 烧录次数
        header.setSectionResizeMode(8, QHeaderView.Fixed)  # 成功率
        
        self.user_table.setColumnWidth(0, 60)
        self.user_table.setColumnWidth(1, 120)
        self.user_table.setColumnWidth(3, 100)
        self.user_table.setColumnWidth(4, 80)
        self.user_table.setColumnWidth(5, 120)
        self.user_table.setColumnWidth(6, 120)
        self.user_table.setColumnWidth(7, 80)
        self.user_table.setColumnWidth(8, 80)
        
        # 右键菜单
        self.user_table.setContextMenuPolicy(Qt.CustomContextMenu)
        self.user_table.customContextMenuRequested.connect(self.show_user_context_menu)
    
    def create_session_tab(self) -> QWidget:
        """创建在线用户标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # 在线用户表格
        self.session_table = QTableWidget()
        columns = ["用户名", "姓名", "IP地址", "登录时间", "过期时间", "操作"]
        self.session_table.setColumnCount(len(columns))
        self.session_table.setHorizontalHeaderLabels(columns)

        # 设置列宽
        self.session_table.setColumnWidth(0, 120)  # 用户名
        self.session_table.setColumnWidth(1, 140)  # 姓名
        self.session_table.setColumnWidth(2, 140)  # IP地址
        self.session_table.setColumnWidth(3, 160)  # 登录时间
        self.session_table.setColumnWidth(4, 120)  # 过期时间
        self.session_table.setColumnWidth(5, 150)  # 操作

        # 设置表格属性
        self.session_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.session_table.setAlternatingRowColors(True)
        self.session_table.verticalHeader().setVisible(False)
        self.session_table.horizontalHeader().setStretchLastSection(False)

        # 设置行高
        self.session_table.verticalHeader().setDefaultSectionSize(50)
        
        layout.addWidget(self.session_table)
        
        return widget
    
    def create_record_tab(self) -> QWidget:
        """创建烧录记录标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # 搜索工具栏
        search_layout = QHBoxLayout()
        
        self.record_search_edit = QLineEdit()
        self.record_search_edit.setPlaceholderText("搜索设备ID、MAC地址或用户...")
        self.record_search_edit.setMaximumWidth(300)
        search_layout.addWidget(QLabel("搜索:"))
        search_layout.addWidget(self.record_search_edit)
        
        # 状态过滤
        self.status_filter_combo = QComboBox()
        self.status_filter_combo.addItems(["全部", "成功", "失败", "进行中"])
        search_layout.addWidget(QLabel("状态:"))
        search_layout.addWidget(self.status_filter_combo)
        
        search_layout.addStretch()
        layout.addLayout(search_layout)
        
        # 烧录记录表格
        self.record_table = QTableWidget()
        columns = ["ID", "用户", "设备ID", "MAC地址", "设备类型", "状态", "开始时间", "耗时", "错误信息"]
        self.record_table.setColumnCount(len(columns))
        self.record_table.setHorizontalHeaderLabels(columns)
        
        self.record_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.record_table.setAlternatingRowColors(True)
        self.record_table.verticalHeader().setVisible(False)
        
        layout.addWidget(self.record_table)
        
        return widget
    
    def create_stats_tab(self) -> QWidget:
        """创建统计信息标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # 统计信息显示区域
        self.stats_text = QTextEdit()
        self.stats_text.setReadOnly(True)
        self.stats_text.setFont(QFont("Consolas", 10))
        layout.addWidget(self.stats_text)
        
        return widget
    
    def setup_connections(self):
        """设置信号连接"""
        self.search_edit.textChanged.connect(self.search_users)
        self.record_search_edit.textChanged.connect(self.search_records)
        self.status_filter_combo.currentTextChanged.connect(self.filter_records)
        
        # 定时刷新在线用户
        self.session_timer = QTimer()
        self.session_timer.timeout.connect(self.load_sessions)
        self.session_timer.start(60000)  # 60秒刷新一次
    
    def load_data(self):
        """加载所有数据"""
        self.load_users()
        self.load_sessions()
        self.load_records()
        self.load_statistics()
    
    def load_users(self):
        """加载用户列表"""
        try:
            users = self.user_manager.get_all_users()
            self.user_table.setRowCount(len(users))
            
            for row, user in enumerate(users):
                # ID
                self.user_table.setItem(row, 0, QTableWidgetItem(str(user['id'])))
                
                # 用户名
                self.user_table.setItem(row, 1, QTableWidgetItem(user['username']))
                
                # 邮箱
                self.user_table.setItem(row, 2, QTableWidgetItem(user['email'] or ""))
                
                # 姓名
                self.user_table.setItem(row, 3, QTableWidgetItem(user['real_name'] or ""))
                
                # 状态
                status_item = QTableWidgetItem(user['status'])
                if user['status'] == 'active':
                    status_item.setBackground(QColor(144, 238, 144))  # 浅绿色
                elif user['status'] == 'banned':
                    status_item.setBackground(QColor(255, 182, 193))  # 浅红色
                else:
                    status_item.setBackground(QColor(211, 211, 211))  # 浅灰色
                self.user_table.setItem(row, 4, status_item)
                
                # 创建时间
                created_at = user['created_at'].strftime("%Y-%m-%d") if user['created_at'] else ""
                self.user_table.setItem(row, 5, QTableWidgetItem(created_at))
                
                # 最后登录
                last_login = user['last_login_at'].strftime("%Y-%m-%d") if user['last_login_at'] else "从未登录"
                self.user_table.setItem(row, 6, QTableWidgetItem(last_login))
                
                # 烧录次数
                total_flashes = user.get('total_flashes', 0) or 0
                self.user_table.setItem(row, 7, QTableWidgetItem(str(total_flashes)))
                
                # 成功率
                successful = user.get('successful_flashes', 0) or 0
                success_rate = f"{(successful/total_flashes*100):.1f}%" if total_flashes > 0 else "0%"
                self.user_table.setItem(row, 8, QTableWidgetItem(success_rate))
                
        except Exception as e:
            logger.error(f"加载用户列表失败: {e}")
            QMessageBox.critical(self, "错误", f"加载用户列表失败: {e}")
    
    def load_sessions(self):
        """加载在线用户"""
        try:
            sessions = self.auth_manager.get_active_sessions()
            self.session_table.setRowCount(len(sessions))
            
            for row, session in enumerate(sessions):
                # 用户名
                self.session_table.setItem(row, 0, QTableWidgetItem(session['username']))
                
                # 姓名
                self.session_table.setItem(row, 1, QTableWidgetItem(session['real_name'] or ""))
                
                # IP地址
                self.session_table.setItem(row, 2, QTableWidgetItem(session['ip_address'] or ""))
                
                # 登录时间
                created_at = session['created_at'].strftime("%Y-%m-%d %H:%M") if session['created_at'] else ""
                self.session_table.setItem(row, 3, QTableWidgetItem(created_at))
                
                # 过期时间（永不过期）
                self.session_table.setItem(row, 4, QTableWidgetItem("永不过期"))
                
                # 操作按钮
                logout_button = QPushButton("强制下线")
                logout_button.setFixedSize(120, 30)
                logout_button.setStyleSheet("""
                    QPushButton {
                        background-color: #e74c3c;
                        color: white;
                        border: none;
                        border-radius: 4px;
                        font-weight: bold;
                        font-size: 12px;
                    }
                    QPushButton:hover {
                        background-color: #c0392b;
                    }
                    QPushButton:pressed {
                        background-color: #a93226;
                    }
                """)
                logout_button.clicked.connect(lambda checked, token=session['session_token']: self.force_logout(token))

                # 创建按钮容器，居中显示
                button_widget = QWidget()
                button_layout = QHBoxLayout(button_widget)
                button_layout.addWidget(logout_button)
                button_layout.setContentsMargins(10, 5, 10, 5)
                button_layout.setAlignment(Qt.AlignCenter)

                self.session_table.setCellWidget(row, 5, button_widget)
                
        except Exception as e:
            logger.error(f"加载在线用户失败: {e}")
    
    def load_records(self):
        """加载烧录记录"""
        try:
            records = self.flash_logger.get_all_flash_records(limit=200)
            self.record_table.setRowCount(len(records))
            
            for row, record in enumerate(records):
                # ID
                self.record_table.setItem(row, 0, QTableWidgetItem(str(record['id'])))
                
                # 用户
                username = record.get('username', '')
                self.record_table.setItem(row, 1, QTableWidgetItem(username))
                
                # 设备ID
                self.record_table.setItem(row, 2, QTableWidgetItem(record['device_id']))
                
                # MAC地址
                self.record_table.setItem(row, 3, QTableWidgetItem(record['device_mac'] or ""))
                
                # 设备类型
                self.record_table.setItem(row, 4, QTableWidgetItem(record['device_type'] or ""))
                
                # 状态
                status_item = QTableWidgetItem(record['flash_status'])
                if record['flash_status'] == 'success':
                    status_item.setBackground(QColor(144, 238, 144))
                elif record['flash_status'] == 'failed':
                    status_item.setBackground(QColor(255, 182, 193))
                else:
                    status_item.setBackground(QColor(255, 255, 0))
                self.record_table.setItem(row, 5, status_item)
                
                # 开始时间
                start_time = record['start_time'].strftime("%Y-%m-%d %H:%M") if record['start_time'] else ""
                self.record_table.setItem(row, 6, QTableWidgetItem(start_time))
                
                # 耗时
                duration = f"{record['duration']}秒" if record['duration'] else ""
                self.record_table.setItem(row, 7, QTableWidgetItem(duration))
                
                # 错误信息
                error_msg = record['error_message'] or ""
                if len(error_msg) > 50:
                    error_msg = error_msg[:50] + "..."
                self.record_table.setItem(row, 8, QTableWidgetItem(error_msg))
                
        except Exception as e:
            logger.error(f"加载烧录记录失败: {e}")
    
    def load_statistics(self):
        """加载统计信息"""
        try:
            stats = self.flash_logger.get_flash_statistics(days=30)
            
            if not stats:
                self.stats_text.setText("暂无统计数据")
                return
            
            total_stats = stats.get('total_stats', {})
            daily_stats = stats.get('daily_stats', [])
            user_ranking = stats.get('user_ranking', [])
            
            # 安全获取统计数据
            total_records = total_stats.get('total_records', 0) or 0
            successful_records = total_stats.get('successful_records', 0) or 0
            failed_records = total_stats.get('failed_records', 0) or 0
            active_users = total_stats.get('active_users', 0) or 0
            unique_devices = total_stats.get('unique_devices', 0) or 0
            avg_duration = total_stats.get('avg_success_duration', 0) or 0

            # 计算成功率
            success_rate = (successful_records / max(total_records, 1)) * 100 if total_records > 0 else 0

            # 构建统计报告
            report = f"""
烧录统计报告 (最近30天)
{'='*50}

总体统计:
  总烧录次数: {total_records}
  成功次数: {successful_records}
  失败次数: {failed_records}
  成功率: {success_rate:.1f}%
  活跃用户: {active_users}
  设备数量: {unique_devices}
  平均耗时: {avg_duration:.1f}秒

用户排行榜:
"""
            
            for i, user in enumerate(user_ranking[:10], 1):
                user_total = user.get('user_total', 0) or 0
                user_success = user.get('user_success', 0) or 0
                username = user.get('username', '') or ''
                success_rate = (user_success / max(user_total, 1)) * 100 if user_total > 0 else 0
                report += f"  {i:2d}. {username:<15} {user_total:>4}次 ({success_rate:.1f}%)\n"
            
            if daily_stats:
                report += f"\n最近7天趋势:\n"
                for daily in daily_stats[:7]:
                    date = daily['flash_date'].strftime("%m-%d") if daily.get('flash_date') else ""
                    total = daily.get('daily_total', 0) or 0
                    success = daily.get('daily_success', 0) or 0
                    rate = (success / max(total, 1)) * 100 if total > 0 else 0
                    report += f"  {date}: {total:>3}次 (成功率 {rate:.1f}%)\n"
            
            self.stats_text.setText(report)
            
        except Exception as e:
            logger.error(f"加载统计信息失败: {e}")
            self.stats_text.setText(f"加载统计信息失败: {e}")
    
    def get_dialog_style(self) -> str:
        """获取对话框样式"""
        return """
            QDialog {
                background-color: #f8f9fa;
            }
            
            QTableWidget {
                gridline-color: #dee2e6;
                background-color: white;
                alternate-background-color: #f8f9fa;
                border: 1px solid #dee2e6;
                border-radius: 4px;
            }
            
            QTableWidget::item {
                padding: 8px;
                border-bottom: 1px solid #e9ecef;
            }
            
            QTableWidget::item:selected {
                background-color: #e3f2fd;
                color: #1565c0;
            }
            
            QTableWidget QHeaderView::section {
                background-color: #f1f3f4;
                padding: 8px;
                border: none;
                border-bottom: 2px solid #dee2e6;
                font-weight: bold;
                color: #495057;
            }
            
            QPushButton {
                background-color: #3498db;
                border: none;
                color: white;
                padding: 8px 16px;
                border-radius: 4px;
                font-weight: bold;
            }
            
            QPushButton:hover {
                background-color: #2980b9;
            }
            
            QPushButton:pressed {
                background-color: #21618c;
            }
            
            QLineEdit {
                border: 1px solid #ced4da;
                border-radius: 4px;
                padding: 8px;
                background-color: white;
            }
            
            QLineEdit:focus {
                border-color: #3498db;
            }
            
            QComboBox {
                border: 1px solid #ced4da;
                border-radius: 4px;
                padding: 8px;
                background-color: white;
            }
            
            QTextEdit {
                border: 1px solid #ced4da;
                border-radius: 4px;
                background-color: white;
                font-family: 'Consolas', 'Monaco', monospace;
            }
        """

    def search_users(self):
        """搜索用户"""
        keyword = self.search_edit.text().strip()
        if not keyword:
            self.load_users()
            return

        try:
            users = self.user_manager.search_users(keyword)
            self.user_table.setRowCount(len(users))

            for row, user in enumerate(users):
                # 填充用户数据（与load_users相同的逻辑）
                self.user_table.setItem(row, 0, QTableWidgetItem(str(user['id'])))
                self.user_table.setItem(row, 1, QTableWidgetItem(user['username']))
                self.user_table.setItem(row, 2, QTableWidgetItem(user['email'] or ""))
                self.user_table.setItem(row, 3, QTableWidgetItem(user['real_name'] or ""))

                status_item = QTableWidgetItem(user['status'])
                if user['status'] == 'active':
                    status_item.setBackground(QColor(144, 238, 144))
                elif user['status'] == 'banned':
                    status_item.setBackground(QColor(255, 182, 193))
                else:
                    status_item.setBackground(QColor(211, 211, 211))
                self.user_table.setItem(row, 4, status_item)

                created_at = user['created_at'].strftime("%Y-%m-%d") if user['created_at'] else ""
                self.user_table.setItem(row, 5, QTableWidgetItem(created_at))

                last_login = user['last_login_at'].strftime("%Y-%m-%d") if user['last_login_at'] else "从未登录"
                self.user_table.setItem(row, 6, QTableWidgetItem(last_login))

                total_flashes = user.get('total_flashes', 0) or 0
                self.user_table.setItem(row, 7, QTableWidgetItem(str(total_flashes)))

                successful = user.get('successful_flashes', 0) or 0
                success_rate = f"{(successful/total_flashes*100):.1f}%" if total_flashes > 0 else "0%"
                self.user_table.setItem(row, 8, QTableWidgetItem(success_rate))

        except Exception as e:
            logger.error(f"搜索用户失败: {e}")

    def search_records(self):
        """搜索烧录记录"""
        keyword = self.record_search_edit.text().strip()
        if not keyword:
            self.load_records()
            return

        try:
            records = self.flash_logger.search_flash_records(keyword)
            self.populate_record_table(records)
        except Exception as e:
            logger.error(f"搜索烧录记录失败: {e}")

    def filter_records(self):
        """过滤烧录记录"""
        status_filter = self.status_filter_combo.currentText()
        if status_filter == "全部":
            self.load_records()
            return

        status_map = {"成功": "success", "失败": "failed", "进行中": "in_progress"}
        status = status_map.get(status_filter)

        if status:
            try:
                records = self.flash_logger.get_all_flash_records(limit=200, status_filter=status)
                self.populate_record_table(records)
            except Exception as e:
                logger.error(f"过滤烧录记录失败: {e}")

    def populate_record_table(self, records):
        """填充烧录记录表格"""
        self.record_table.setRowCount(len(records))

        for row, record in enumerate(records):
            self.record_table.setItem(row, 0, QTableWidgetItem(str(record['id'])))

            username = record.get('username', '')
            self.record_table.setItem(row, 1, QTableWidgetItem(username))

            self.record_table.setItem(row, 2, QTableWidgetItem(record['device_id']))
            self.record_table.setItem(row, 3, QTableWidgetItem(record['device_mac'] or ""))
            self.record_table.setItem(row, 4, QTableWidgetItem(record['device_type'] or ""))

            status_item = QTableWidgetItem(record['flash_status'])
            if record['flash_status'] == 'success':
                status_item.setBackground(QColor(144, 238, 144))
            elif record['flash_status'] == 'failed':
                status_item.setBackground(QColor(255, 182, 193))
            else:
                status_item.setBackground(QColor(255, 255, 0))
            self.record_table.setItem(row, 5, status_item)

            start_time = record['start_time'].strftime("%Y-%m-%d %H:%M") if record['start_time'] else ""
            self.record_table.setItem(row, 6, QTableWidgetItem(start_time))

            duration = f"{record['duration']}秒" if record['duration'] else ""
            self.record_table.setItem(row, 7, QTableWidgetItem(duration))

            error_msg = record['error_message'] or ""
            if len(error_msg) > 50:
                error_msg = error_msg[:50] + "..."
            self.record_table.setItem(row, 8, QTableWidgetItem(error_msg))

    def show_user_context_menu(self, position):
        """显示用户右键菜单"""
        item = self.user_table.itemAt(position)
        if not item:
            return

        row = item.row()
        user_id = int(self.user_table.item(row, 0).text())
        username = self.user_table.item(row, 1).text()
        status = self.user_table.item(row, 4).text()

        menu = QMenu(self)

        # 编辑用户
        edit_action = QAction("编辑用户", self)
        edit_action.triggered.connect(lambda: self.edit_user(user_id))
        menu.addAction(edit_action)

        # 修改密码
        password_action = QAction("修改密码", self)
        password_action.triggered.connect(lambda: self.change_password(user_id, username))
        menu.addAction(password_action)

        menu.addSeparator()

        # 封禁/解封
        if status == 'active':
            ban_action = QAction("封禁用户", self)
            ban_action.triggered.connect(lambda: self.ban_user(user_id, username))
            menu.addAction(ban_action)
        elif status == 'banned':
            unban_action = QAction("解封用户", self)
            unban_action.triggered.connect(lambda: self.unban_user(user_id, username))
            menu.addAction(unban_action)

        # 强制下线
        logout_action = QAction("强制下线", self)
        logout_action.triggered.connect(lambda: self.force_logout_user(user_id, username))
        menu.addAction(logout_action)

        menu.addSeparator()

        # 查看烧录记录
        records_action = QAction("查看烧录记录", self)
        records_action.triggered.connect(lambda: self.view_user_records(user_id, username))
        menu.addAction(records_action)

        # 删除用户
        if status != 'deleted':
            delete_action = QAction("删除用户", self)
            delete_action.triggered.connect(lambda: self.delete_user(user_id, username))
            menu.addAction(delete_action)

        menu.exec(self.user_table.mapToGlobal(position))

    def add_user(self):
        """添加用户"""
        dialog = AddUserDialog(self)
        if dialog.exec() == QDialog.Accepted:
            user_data = dialog.get_user_data()
            if self.user_manager.create_user(**user_data):
                QMessageBox.information(self, "成功", "用户创建成功")
                self.load_users()
            else:
                QMessageBox.critical(self, "错误", "用户创建失败")

    def edit_user(self, user_id: int):
        """编辑用户"""
        user = self.user_manager.get_user_by_id(user_id)
        if not user:
            QMessageBox.critical(self, "错误", "用户不存在")
            return

        dialog = EditUserDialog(user, self)
        if dialog.exec() == QDialog.Accepted:
            user_data = dialog.get_user_data()
            if self.user_manager.update_user(user_id, **user_data):
                QMessageBox.information(self, "成功", "用户信息更新成功")
                self.load_users()
            else:
                QMessageBox.critical(self, "错误", "用户信息更新失败")

    def change_password(self, user_id: int, username: str):
        """修改密码"""
        password, ok = QInputDialog.getText(self, "修改密码",
                                          f"为用户 {username} 设置新密码:",
                                          QLineEdit.Password)
        if ok and password:
            if len(password) < 6:
                QMessageBox.warning(self, "警告", "密码长度至少6位")
                return

            if self.user_manager.change_password(user_id, password):
                QMessageBox.information(self, "成功", "密码修改成功，用户需要重新登录")
            else:
                QMessageBox.critical(self, "错误", "密码修改失败")

    def ban_user(self, user_id: int, username: str):
        """封禁用户"""
        reply = QMessageBox.question(self, "确认", f"确定要封禁用户 {username} 吗？",
                                   QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            if self.user_manager.ban_user(user_id):
                QMessageBox.information(self, "成功", f"用户 {username} 已被封禁")
                self.load_users()
                self.load_sessions()
            else:
                QMessageBox.critical(self, "错误", "封禁用户失败")

    def unban_user(self, user_id: int, username: str):
        """解封用户"""
        reply = QMessageBox.question(self, "确认", f"确定要解封用户 {username} 吗？",
                                   QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            if self.user_manager.unban_user(user_id):
                QMessageBox.information(self, "成功", f"用户 {username} 已解封")
                self.load_users()
            else:
                QMessageBox.critical(self, "错误", "解封用户失败")

    def delete_user(self, user_id: int, username: str):
        """删除用户"""
        reply = QMessageBox.question(self, "确认", f"确定要删除用户 {username} 吗？\n此操作不可撤销！",
                                   QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            if self.user_manager.delete_user(user_id):
                QMessageBox.information(self, "成功", f"用户 {username} 已删除")
                self.load_users()
                self.load_sessions()
            else:
                QMessageBox.critical(self, "错误", "删除用户失败")

    def force_logout_user(self, user_id: int, username: str):
        """强制用户下线"""
        reply = QMessageBox.question(self, "确认", f"确定要强制用户 {username} 下线吗？",
                                   QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            if self.user_manager.force_logout_user(user_id):
                QMessageBox.information(self, "成功", f"用户 {username} 已被强制下线")
                self.load_sessions()
            else:
                QMessageBox.critical(self, "错误", "强制下线失败")

    def force_logout(self, session_token: str):
        """强制会话下线"""
        if self.auth_manager.logout_session(session_token):
            QMessageBox.information(self, "成功", "用户已被强制下线")
            self.load_sessions()
        else:
            QMessageBox.critical(self, "错误", "强制下线失败")

    def view_user_records(self, user_id: int, username: str):
        """查看用户烧录记录"""
        records = self.flash_logger.get_user_flash_records(user_id, limit=100)

        dialog = QDialog(self)
        dialog.setWindowTitle(f"用户 {username} 的烧录记录")
        dialog.setMinimumSize(800, 600)

        layout = QVBoxLayout(dialog)

        table = QTableWidget()
        columns = ["ID", "设备ID", "MAC地址", "设备类型", "状态", "开始时间", "耗时", "错误信息"]
        table.setColumnCount(len(columns))
        table.setHorizontalHeaderLabels(columns)
        table.setRowCount(len(records))

        for row, record in enumerate(records):
            table.setItem(row, 0, QTableWidgetItem(str(record['id'])))
            table.setItem(row, 1, QTableWidgetItem(record['device_id']))
            table.setItem(row, 2, QTableWidgetItem(record['device_mac'] or ""))
            table.setItem(row, 3, QTableWidgetItem(record['device_type'] or ""))

            status_item = QTableWidgetItem(record['flash_status'])
            if record['flash_status'] == 'success':
                status_item.setBackground(QColor(144, 238, 144))
            elif record['flash_status'] == 'failed':
                status_item.setBackground(QColor(255, 182, 193))
            table.setItem(row, 4, status_item)

            start_time = record['start_time'].strftime("%Y-%m-%d %H:%M") if record['start_time'] else ""
            table.setItem(row, 5, QTableWidgetItem(start_time))

            duration = f"{record['duration']}秒" if record['duration'] else ""
            table.setItem(row, 6, QTableWidgetItem(duration))

            table.setItem(row, 7, QTableWidgetItem(record['error_message'] or ""))

        table.setAlternatingRowColors(True)
        table.verticalHeader().setVisible(False)
        layout.addWidget(table)

        close_button = QPushButton("关闭")
        close_button.clicked.connect(dialog.accept)
        layout.addWidget(close_button)

        dialog.exec()

class AddUserDialog(QDialog):
    """添加用户对话框"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setup_ui()
        self.setWindowTitle("添加用户")
        self.setFixedSize(400, 300)

    def setup_ui(self):
        layout = QVBoxLayout(self)

        form_layout = QFormLayout()

        self.username_edit = QLineEdit()
        self.username_edit.setPlaceholderText("用户名")
        form_layout.addRow("用户名*:", self.username_edit)

        self.password_edit = QLineEdit()
        self.password_edit.setPlaceholderText("密码")
        self.password_edit.setEchoMode(QLineEdit.Password)
        form_layout.addRow("密码*:", self.password_edit)

        self.email_edit = QLineEdit()
        self.email_edit.setPlaceholderText("邮箱地址")
        form_layout.addRow("邮箱:", self.email_edit)

        self.real_name_edit = QLineEdit()
        self.real_name_edit.setPlaceholderText("真实姓名")
        form_layout.addRow("姓名:", self.real_name_edit)

        self.status_combo = QComboBox()
        self.status_combo.addItems(["active", "banned"])
        form_layout.addRow("状态:", self.status_combo)

        layout.addLayout(form_layout)

        button_layout = QHBoxLayout()

        self.cancel_button = QPushButton("取消")
        self.cancel_button.clicked.connect(self.reject)
        button_layout.addWidget(self.cancel_button)

        self.ok_button = QPushButton("确定")
        self.ok_button.clicked.connect(self.accept)
        button_layout.addWidget(self.ok_button)

        layout.addLayout(button_layout)

    def get_user_data(self):
        return {
            'username': self.username_edit.text().strip(),
            'password': self.password_edit.text(),
            'email': self.email_edit.text().strip(),
            'real_name': self.real_name_edit.text().strip(),
            'status': self.status_combo.currentText()
        }

class EditUserDialog(QDialog):
    """编辑用户对话框"""

    def __init__(self, user_data: Dict[str, Any], parent=None):
        super().__init__(parent)
        self.user_data = user_data
        self.setup_ui()
        self.load_user_data()
        self.setWindowTitle("编辑用户")
        self.setFixedSize(400, 250)

    def setup_ui(self):
        layout = QVBoxLayout(self)

        form_layout = QFormLayout()

        self.email_edit = QLineEdit()
        form_layout.addRow("邮箱:", self.email_edit)

        self.real_name_edit = QLineEdit()
        form_layout.addRow("姓名:", self.real_name_edit)

        self.status_combo = QComboBox()
        self.status_combo.addItems(["active", "banned"])
        form_layout.addRow("状态:", self.status_combo)

        layout.addLayout(form_layout)

        button_layout = QHBoxLayout()

        self.cancel_button = QPushButton("取消")
        self.cancel_button.clicked.connect(self.reject)
        button_layout.addWidget(self.cancel_button)

        self.ok_button = QPushButton("确定")
        self.ok_button.clicked.connect(self.accept)
        button_layout.addWidget(self.ok_button)

        layout.addLayout(button_layout)

    def load_user_data(self):
        self.email_edit.setText(self.user_data.get('email', ''))
        self.real_name_edit.setText(self.user_data.get('real_name', ''))
        self.status_combo.setCurrentText(self.user_data.get('status', 'active'))

    def get_user_data(self):
        return {
            'email': self.email_edit.text().strip(),
            'real_name': self.real_name_edit.text().strip(),
            'status': self.status_combo.currentText()
        }
