#!/usr/bin/env python3
import sys
import logging
from typing import Optional, Dict, Any
from PySide6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QFormLayout,
                              QLineEdit, QPushButton, QLabel, QCheckBox, 
                              QMessageBox, QFrame, QApplication)
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QFont, QPixmap, QIcon
from auth import get_auth_manager

logger = logging.getLogger("xiaozhi.login")

class LoginDialog(QDialog):
    """用户登录对话框"""
    
    # 登录成功信号
    login_success = Signal(dict)  # 传递用户信息
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.auth_manager = get_auth_manager()
        self.user_info = None
        
        self.setup_ui()
        self.setup_connections()
        
        # 设置窗口属性
        self.setWindowTitle("小乔智能设备烧录工具 - 用户登录")
        self.setMinimumSize(450, 350)
        self.setMaximumSize(600, 500)
        self.resize(450, 350)
        self.setWindowFlags(Qt.Dialog | Qt.WindowCloseButtonHint)
        self.setModal(True)
        
        # 居中显示
        self.center_on_screen()
        
        logger.info("登录对话框初始化完成")
    
    def setup_ui(self):
        """设置用户界面"""
        layout = QVBoxLayout(self)
        layout.setSpacing(15)
        layout.setContentsMargins(40, 30, 40, 30)
        
        # 标题区域
        title_layout = QVBoxLayout()
        
        # 应用标题
        title_label = QLabel("小乔智能设备烧录工具")
        title_label.setAlignment(Qt.AlignCenter)
        title_font = QFont()
        title_font.setPointSize(16)
        title_font.setBold(True)
        title_label.setFont(title_font)
        title_label.setStyleSheet("color: #2c3e50; margin-bottom: 10px;")
        title_layout.addWidget(title_label)
        
        # 副标题
        subtitle_label = QLabel("请登录以继续使用")
        subtitle_label.setAlignment(Qt.AlignCenter)
        subtitle_label.setStyleSheet("color: #7f8c8d; font-size: 12px;")
        title_layout.addWidget(subtitle_label)
        
        layout.addLayout(title_layout)
        
        # 分隔线
        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setFrameShadow(QFrame.Sunken)
        line.setStyleSheet("color: #bdc3c7;")
        layout.addWidget(line)
        
        # 登录表单
        form_layout = QFormLayout()
        form_layout.setSpacing(15)
        
        # 用户名输入
        self.username_edit = QLineEdit()
        self.username_edit.setPlaceholderText("请输入用户名")
        self.username_edit.setMinimumHeight(35)
        form_layout.addRow("用户名:", self.username_edit)
        
        # 密码输入
        self.password_edit = QLineEdit()
        self.password_edit.setPlaceholderText("请输入密码")
        self.password_edit.setEchoMode(QLineEdit.Password)
        self.password_edit.setMinimumHeight(35)
        form_layout.addRow("密码:", self.password_edit)
        
        layout.addLayout(form_layout)
        
        # 记住密码选项
        self.remember_checkbox = QCheckBox("记住用户名")
        self.remember_checkbox.setStyleSheet("color: #7f8c8d;")
        layout.addWidget(self.remember_checkbox)
        
        # 按钮区域
        button_layout = QHBoxLayout()
        button_layout.setSpacing(15)
        button_layout.setContentsMargins(0, 10, 0, 0)

        # 添加弹性空间
        button_layout.addStretch()

        # 取消按钮
        self.cancel_button = QPushButton("取消")
        self.cancel_button.setMinimumHeight(40)
        self.cancel_button.setMinimumWidth(100)
        self.cancel_button.setStyleSheet(self.get_button_style("cancel"))
        button_layout.addWidget(self.cancel_button)

        # 登录按钮
        self.login_button = QPushButton("登录")
        self.login_button.setMinimumHeight(40)
        self.login_button.setMinimumWidth(100)
        self.login_button.setDefault(True)
        self.login_button.setStyleSheet(self.get_button_style("login"))
        button_layout.addWidget(self.login_button)

        # 添加弹性空间
        button_layout.addStretch()

        layout.addLayout(button_layout)
        
        # 状态标签
        self.status_label = QLabel("")
        self.status_label.setAlignment(Qt.AlignCenter)
        self.status_label.setStyleSheet("color: #e74c3c; font-size: 12px;")
        self.status_label.hide()
        layout.addWidget(self.status_label)
        
        # 应用样式
        self.setStyleSheet(self.get_dialog_style())
        
        # 加载保存的用户名
        self.load_saved_username()
    
    def setup_connections(self):
        """设置信号连接"""
        self.login_button.clicked.connect(self.handle_login)
        self.cancel_button.clicked.connect(self.reject)
        self.username_edit.returnPressed.connect(self.password_edit.setFocus)
        self.password_edit.returnPressed.connect(self.handle_login)
    
    def get_button_style(self, button_type: str) -> str:
        """获取按钮样式"""
        if button_type == "login":
            return """
                QPushButton {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #3498db, stop: 1 #2980b9);
                    border: none;
                    color: white;
                    font-weight: bold;
                    border-radius: 6px;
                }
                QPushButton:hover {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #2980b9, stop: 1 #21618c);
                }
                QPushButton:pressed {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #21618c, stop: 1 #1b4f72);
                }
            """
        else:  # cancel
            return """
                QPushButton {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #95a5a6, stop: 1 #7f8c8d);
                    border: none;
                    color: white;
                    font-weight: bold;
                    border-radius: 6px;
                }
                QPushButton:hover {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #7f8c8d, stop: 1 #6c7b7d);
                }
                QPushButton:pressed {
                    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                              stop: 0 #6c7b7d, stop: 1 #566573);
                }
            """
    
    def get_dialog_style(self) -> str:
        """获取对话框样式"""
        return """
            QDialog {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                          stop: 0 #ffffff, stop: 1 #f8f9fa);
                border: 1px solid #dee2e6;
                border-radius: 10px;
            }
            
            QLineEdit {
                border: 2px solid #ced4da;
                border-radius: 6px;
                padding: 8px 12px;
                font-size: 12px;
                background-color: white;
            }
            
            QLineEdit:focus {
                border-color: #3498db;
                outline: none;
            }
            
            QLabel {
                color: #495057;
            }
            
            QCheckBox {
                color: #495057;
            }
            
            QCheckBox::indicator {
                width: 16px;
                height: 16px;
                border-radius: 3px;
                border: 2px solid #ced4da;
                background-color: white;
            }
            
            QCheckBox::indicator:checked {
                background-color: #3498db;
                border-color: #2980b9;
            }
        """
    
    def center_on_screen(self):
        """居中显示对话框"""
        screen = QApplication.primaryScreen().geometry()
        dialog_geometry = self.geometry()
        x = (screen.width() - dialog_geometry.width()) // 2
        y = (screen.height() - dialog_geometry.height()) // 2
        self.move(x, y)
    
    def show_status(self, message: str, is_error: bool = True):
        """显示状态信息"""
        self.status_label.setText(message)
        if is_error:
            self.status_label.setStyleSheet("color: #e74c3c; font-size: 12px;")
        else:
            self.status_label.setStyleSheet("color: #27ae60; font-size: 12px;")
        self.status_label.show()
    
    def hide_status(self):
        """隐藏状态信息"""
        self.status_label.hide()
    
    def handle_login(self):
        """处理登录"""
        username = self.username_edit.text().strip()
        password = self.password_edit.text()
        
        # 验证输入
        if not username:
            self.show_status("请输入用户名")
            self.username_edit.setFocus()
            return
        
        if not password:
            self.show_status("请输入密码")
            self.password_edit.setFocus()
            return
        
        # 禁用按钮，显示登录中状态
        self.login_button.setEnabled(False)
        self.login_button.setText("登录中...")
        self.hide_status()
        
        try:
            # 尝试登录
            user_info = self.auth_manager.login(username, password)
            
            if user_info:
                # 登录成功
                self.user_info = user_info
                self.show_status("登录成功", False)
                
                # 保存用户名（如果选择了记住）
                if self.remember_checkbox.isChecked():
                    self.save_username(username)
                else:
                    self.clear_saved_username()
                
                # 发送登录成功信号
                self.login_success.emit(user_info)
                
                # 延迟关闭对话框
                QApplication.processEvents()
                self.accept()
            else:
                # 登录失败
                self.show_status("用户名或密码错误，或账户已被禁用")
                self.password_edit.clear()
                self.password_edit.setFocus()
                
        except Exception as e:
            logger.error(f"登录异常: {e}")
            self.show_status("登录失败，请检查网络连接或联系管理员")
        
        finally:
            # 恢复按钮状态
            self.login_button.setEnabled(True)
            self.login_button.setText("登录")
    
    def save_username(self, username: str):
        """保存用户名到配置"""
        try:
            import configparser
            config = configparser.ConfigParser()
            config.read('config.ini', encoding='utf-8')
            
            if not config.has_section('login'):
                config.add_section('login')
            
            config.set('login', 'saved_username', username)
            
            with open('config.ini', 'w', encoding='utf-8') as f:
                config.write(f)
        except Exception as e:
            logger.warning(f"保存用户名失败: {e}")
    
    def load_saved_username(self):
        """加载保存的用户名"""
        try:
            import configparser
            config = configparser.ConfigParser()
            config.read('config.ini', encoding='utf-8')
            
            if config.has_option('login', 'saved_username'):
                username = config.get('login', 'saved_username')
                if username:
                    self.username_edit.setText(username)
                    self.remember_checkbox.setChecked(True)
                    self.password_edit.setFocus()
        except Exception as e:
            logger.warning(f"加载保存的用户名失败: {e}")
    
    def clear_saved_username(self):
        """清除保存的用户名"""
        try:
            import configparser
            config = configparser.ConfigParser()
            config.read('config.ini', encoding='utf-8')
            
            if config.has_section('login') and config.has_option('login', 'saved_username'):
                config.remove_option('login', 'saved_username')
                
                with open('config.ini', 'w', encoding='utf-8') as f:
                    config.write(f)
        except Exception as e:
            logger.warning(f"清除保存的用户名失败: {e}")
    
    def get_user_info(self) -> Optional[Dict[str, Any]]:
        """获取登录用户信息"""
        return self.user_info

# 测试代码
if __name__ == "__main__":
    app = QApplication(sys.argv)
    
    # 初始化数据库管理器
    from database import init_database_manager
    init_database_manager()
    
    dialog = LoginDialog()
    if dialog.exec() == QDialog.Accepted:
        user_info = dialog.get_user_info()
        print(f"登录成功: {user_info}")
    else:
        print("登录取消")
    
    sys.exit()
