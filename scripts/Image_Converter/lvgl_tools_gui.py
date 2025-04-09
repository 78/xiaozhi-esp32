import sys
import os
import tempfile
import re
import glob
from PIL import Image
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QLabel, QComboBox, 
                             QPushButton, QVBoxLayout, QHBoxLayout, QGridLayout, 
                             QFileDialog, QMessageBox, QGroupBox, QTextEdit, 
                             QTableWidget, QTableWidgetItem, QHeaderView, QCheckBox,
                             QScrollArea, QFrame, QLineEdit, QSizePolicy)
from PySide6.QtCore import Qt, QSize
from PySide6.QtGui import QFont, QIcon, QColor, QPalette
from LVGLImage import LVGLImage, ColorFormat, CompressMethod

HELP_TEXT = """LVGL图片转换工具使用说明：

1. 添加文件：点击"添加文件"按钮选择需要转换的图片，支持批量导入

2. 移除文件：勾选文件列表中的复选框，点击"移除选中"可删除选定文件

3. 设置分辨率：选择需要的分辨率，如32x32, 64x64, 128x128, 256x256或360x360
   建议根据自己的设备的屏幕分辨率来选择。过大和过小都会影响显示效果。

4. 颜色格式：选择"自动识别"会根据图片是否透明自动选择，或手动指定
   除非你了解这个选项，否则建议使用自动识别，不然可能会出现一些意想不到的问题……

5. 压缩方式：选择NONE不压缩、RLE压缩或LZ4压缩
   - NONE：不压缩，文件最大但解码最快
   - RLE：行程长度编码，对重复像素压缩效果好
   - LZ4：通用压缩算法，压缩率高，适合复杂图像

6. 输出目录：设置转换后文件的保存路径
   默认为程序所在目录下的output文件夹

7. 表情功能：点击表情按钮可以选择一张图片，并自动重命名为对应的表情名称

8. 转换：点击"转换全部"或"转换选中"开始转换
"""

# 表情符号列表
EMOJI_LIST = [
    {"code": "1f636_64", "name": "neutral", "description": "平静"},
    {"code": "1f642_64", "name": "happy", "description": "开心"},
    {"code": "1f606_64", "name": "laughing", "description": "大笑"},
    {"code": "1f602_64", "name": "funny", "description": "搞笑"},
    {"code": "1f614_64", "name": "sad", "description": "悲伤"},
    {"code": "1f620_64", "name": "angry", "description": "生气"},
    {"code": "1f62d_64", "name": "crying", "description": "哭泣"},
    {"code": "1f60d_64", "name": "loving", "description": "喜爱"},
    {"code": "1f633_64", "name": "embarrassed", "description": "尴尬"},
    {"code": "1f62f_64", "name": "surprised", "description": "惊讶"},
    {"code": "1f631_64", "name": "shocked", "description": "震惊"},
    {"code": "1f914_64", "name": "thinking", "description": "思考"},
    {"code": "1f609_64", "name": "winking", "description": "眨眼"},
    {"code": "1f60e_64", "name": "cool", "description": "酷"},
    {"code": "1f60c_64", "name": "relaxed", "description": "放松"},
    {"code": "1f924_64", "name": "delicious", "description": "美味"},
    {"code": "1f618_64", "name": "kissy", "description": "亲吻"},
    {"code": "1f60f_64", "name": "confident", "description": "自信"},
    {"code": "1f634_64", "name": "sleepy", "description": "困倦"},
    {"code": "1f61c_64", "name": "silly", "description": "傻傻的"},
    {"code": "1f644_64", "name": "confused", "description": "困惑"}
]

class ImageConverterApp(QMainWindow):
    def __init__(self):
        super().__init__()
        
        # 初始化变量
        self.output_dir = os.path.abspath("output")
        self.resolution = "128x128"
        self.color_format = "自动识别"
        self.compress_method = "NONE"
        self.selected_emoji = {"name": "", "code": "", "description": ""}
        
        # 存储文件列表数据
        self.file_list = []  # 格式: [{"path": path, "selected": True/False, "display_name": name}, ...]
        
        # 跟踪表情按钮和框架
        self.emoji_buttons = []  # 存储所有表情按钮
        self.emoji_frames = []   # 存储所有表情框架
        self.current_selected_button = None  # 当前选中的按钮
        
        # 设置窗口
        self.setWindowTitle("LVGL图片转换工具")
        self.setMinimumSize(1000, 800)
        self.setup_ui()
        
        # 设置样式
        self.setup_style()
    
    def setup_style(self):
        """设置应用样式"""
        # 创建并设置全局字体
        app_font = QFont()
        app_font.setPointSize(10)
        QApplication.setFont(app_font)
        
        # 应用样式表
        style = """
        QMainWindow {
            background-color: #f5f5f5;
        }
        QGroupBox {
            border: 1px solid #dcdcdc;
            border-radius: 6px;
            margin-top: 1.5ex;
            font-weight: bold;
            padding: 10px;
            background-color: white;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            background-color: white;
            color: #333333;
        }
        QPushButton {
            background-color: #4a86e8;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #3a76d8;
        }
        QPushButton:pressed {
            background-color: #2a66c8;
        }
        QTableWidget {
            border: 1px solid #dcdcdc;
            border-radius: 4px;
            gridline-color: #f0f0f0;
            alternate-background-color: #fafafa;
        }
        QTableWidget::item {
            padding: 5px;
        }
        QHeaderView::section {
            background-color: #4a86e8;
            color: white;
            padding: 8px;
            border: none;
            font-weight: bold;
        }
        QTextEdit {
            border: 1px solid #dcdcdc;
            border-radius: 4px;
            padding: 8px;
            background-color: white;
        }
        QLineEdit {
            border: 1px solid #dcdcdc;
            border-radius: 4px;
            padding: 8px;
        }
        QComboBox {
            border: 1px solid #dcdcdc;
            border-radius: 4px;
            padding: 8px;
        }
        QComboBox::drop-down {
            border: 0px;
            width: 20px;
        }
        QScrollArea {
            border: none;
        }
        QFrame {
            border: 1px solid #dcdcdc;
            border-radius: 4px;
        }
        QCheckBox {
            spacing: 8px;
        }
        QScrollBar {
            width: 10px;
            background: #f5f5f5;
        }
        QScrollBar::handle {
            background: #4a86e8;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::add-line, QScrollBar::sub-line {
            height: 0px;
        }
        """
        self.setStyleSheet(style)
    
    def setup_ui(self):
        """创建主UI"""
        # 创建中央窗口部件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # 创建主布局
        main_layout = QVBoxLayout(central_widget)
        main_layout.setSpacing(15)
        main_layout.setContentsMargins(20, 20, 20, 20)
        
        # === 标题栏 ===
        title_label = QLabel("LVGL图片转换工具")
        title_label.setAlignment(Qt.AlignCenter)
        title_font = QFont()
        title_font.setPointSize(16)
        title_font.setBold(True)
        title_label.setFont(title_font)
        title_label.setStyleSheet("color: #4a86e8; margin-bottom: 10px;")
        main_layout.addWidget(title_label)
        
        # === 顶部控制区（横向布局）===
        top_controls = QHBoxLayout()
        top_controls.setSpacing(15)
        
        # --- 左侧设置组 ---
        settings_group = QGroupBox("转换设置")
        settings_layout = QVBoxLayout(settings_group)
        
        # 设置网格
        settings_grid = QGridLayout()
        settings_grid.setColumnStretch(1, 1)
        settings_grid.setSpacing(10)
        
        # 分辨率设置
        settings_grid.addWidget(QLabel("分辨率:"), 0, 0)
        self.res_combo = QComboBox()
        self.res_combo.addItems(["128x128", "64x64", "32x32", "256x256", "360x360"])
        self.res_combo.setCurrentText(self.resolution)
        settings_grid.addWidget(self.res_combo, 0, 1)
        
        # 颜色格式
        settings_grid.addWidget(QLabel("颜色格式:"), 1, 0)
        self.color_combo = QComboBox()
        self.color_combo.addItems(["自动识别", "RGB565", "RGB565A8"])
        self.color_combo.setCurrentText(self.color_format)
        settings_grid.addWidget(self.color_combo, 1, 1)
        
        # 压缩方式
        settings_grid.addWidget(QLabel("压缩方式:"), 2, 0)
        self.compress_combo = QComboBox()
        self.compress_combo.addItems(["NONE", "RLE", "LZ4"])
        self.compress_combo.setCurrentText(self.compress_method)
        settings_grid.addWidget(self.compress_combo, 2, 1)
        
        settings_layout.addLayout(settings_grid)
        
        # --- 右侧输出目录组 ---
        output_group = QGroupBox("输出目录")
        output_layout = QHBoxLayout(output_group)
        output_layout.setContentsMargins(10, 15, 10, 15)
        output_layout.setSpacing(10)
        
        self.output_dir_entry = QLineEdit(self.output_dir)
        browse_output_btn = QPushButton("浏览")
        browse_output_btn.setFixedWidth(80)
        browse_output_btn.clicked.connect(self.select_output_dir)
        output_layout.addWidget(self.output_dir_entry)
        output_layout.addWidget(browse_output_btn)
        
        # 添加左右两侧控件到顶部布局
        top_controls.addWidget(settings_group, 3)
        top_controls.addWidget(output_group, 2)
        
        main_layout.addLayout(top_controls)
        
        # === 文件操作区 ===
        files_group = QGroupBox("输入文件")
        files_layout = QVBoxLayout(files_group)
        files_layout.setContentsMargins(10, 15, 10, 15)
        files_layout.setSpacing(10)
        
        # 文件操作按钮行
        file_btn_layout = QHBoxLayout()
        file_btn_layout.setSpacing(10)
        
        add_file_btn = QPushButton("添加文件")
        add_file_btn.clicked.connect(self.select_files)
        
        remove_file_btn = QPushButton("移除选中")
        remove_file_btn.clicked.connect(self.remove_selected)
        
        clear_files_btn = QPushButton("清空列表")
        clear_files_btn.clicked.connect(self.clear_files)
        
        file_btn_layout.addWidget(add_file_btn)
        file_btn_layout.addWidget(remove_file_btn)
        file_btn_layout.addWidget(clear_files_btn)
        file_btn_layout.addStretch()
        files_layout.addLayout(file_btn_layout)
        
        # 文件列表表格
        self.file_table = QTableWidget(0, 2)
        self.file_table.setHorizontalHeaderLabels(["选中", "文件名"])
        self.file_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Fixed)
        self.file_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.Stretch)
        self.file_table.setColumnWidth(0, 60)
        self.file_table.setAlternatingRowColors(True)
        self.file_table.verticalHeader().setVisible(False)
        self.file_table.setShowGrid(False)
        self.file_table.setSelectionBehavior(QTableWidget.SelectRows)
        self.file_table.verticalHeader().setDefaultSectionSize(36)
        files_layout.addWidget(self.file_table)
        
        main_layout.addWidget(files_group)
        
        # === 表情选择区（简化后的网格布局）===
        emoji_group = QGroupBox("表情选择")
        emoji_layout = QVBoxLayout(emoji_group)
        emoji_layout.setContentsMargins(10, 15, 10, 15)
        
        # 当前选中表情显示
        emoji_info_layout = QHBoxLayout()
        emoji_info_label = QLabel("当前选中表情:")
        self.selected_emoji_label = QLabel("")
        self.selected_emoji_label.setStyleSheet("font-weight: bold; color: #4a86e8;")
        emoji_info_layout.addWidget(emoji_info_label)
        emoji_info_layout.addWidget(self.selected_emoji_label)
        emoji_info_layout.addStretch()
        emoji_layout.addLayout(emoji_info_layout)
        
        # 创建滚动区域
        emoji_scroll = QScrollArea()
        emoji_scroll.setWidgetResizable(True)
        emoji_content = QWidget()
        emoji_grid = QGridLayout(emoji_content)
        emoji_grid.setSpacing(10)
        
        # 清空按钮列表
        self.emoji_buttons = []
        self.emoji_frames = []
        
        # 每行显示5个表情按钮
        columns = 5
        for i, emoji in enumerate(EMOJI_LIST):
            row = i // columns
            col = i % columns
            
            # 创建一个简化的框架
            frame = QFrame()
            frame.setObjectName(f"emoji_frame_{emoji['code']}")
            frame.setStyleSheet("background-color: white; padding: 5px;")
            frame_layout = QVBoxLayout(frame)
            frame_layout.setSpacing(5)
            frame_layout.setContentsMargins(5, 5, 5, 5)
            
            # 表情名称和描述
            label = QLabel(f"{emoji['name']}\n{emoji['description']}")
            label.setAlignment(Qt.AlignCenter)
            label.setWordWrap(True)
            
            # 创建按钮
            btn = QPushButton("选择")
            btn.setObjectName(f"emoji_btn_{emoji['code']}")
            btn.setProperty("emoji_code", emoji["code"])
            btn.setProperty("emoji_name", emoji["name"])
            btn.setProperty("emoji_desc", emoji["description"])
            btn.clicked.connect(self.on_emoji_button_clicked)
            
            # 添加到跟踪列表
            self.emoji_buttons.append(btn)
            self.emoji_frames.append(frame)
            
            frame_layout.addWidget(label)
            frame_layout.addWidget(btn)
            emoji_grid.addWidget(frame, row, col)
        
        emoji_scroll.setWidget(emoji_content)
        emoji_layout.addWidget(emoji_scroll)
        
        main_layout.addWidget(emoji_group)
        
        # === 底部操作和日志区 ===
        bottom_layout = QHBoxLayout()
        
        # --- 左侧操作按钮 ---
        buttons_layout = QVBoxLayout()
        buttons_layout.setSpacing(10)
        
        convert_all_btn = QPushButton("转换全部文件")
        convert_all_btn.clicked.connect(lambda: self.start_conversion(True))
        
        convert_selected_btn = QPushButton("转换选中文件")
        convert_selected_btn.clicked.connect(lambda: self.start_conversion(False))
        
        help_btn = QPushButton("帮助")
        help_btn.clicked.connect(self.show_help)
        
        buttons_layout.addWidget(convert_all_btn)
        buttons_layout.addWidget(convert_selected_btn)
        buttons_layout.addWidget(help_btn)
        buttons_layout.addStretch()
        
        # --- 右侧日志区 ---
        log_group = QGroupBox("日志")
        log_layout = QVBoxLayout(log_group)
        log_layout.setContentsMargins(10, 15, 10, 15)
        
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setStyleSheet("font-family: Consolas, Monaco, monospace;")
        log_layout.addWidget(self.log_text)
        
        # 清空日志按钮
        clear_log_btn = QPushButton("清空日志")
        clear_log_btn.clicked.connect(self.clear_log)
        log_layout.addWidget(clear_log_btn, 0, Qt.AlignRight)
        
        bottom_layout.addLayout(buttons_layout, 1)
        bottom_layout.addWidget(log_group, 3)
        
        main_layout.addLayout(bottom_layout)
    
    def update_emoji_selection_style(self, selected_button=None):
        """更新表情按钮的选中状态样式"""
        for i, btn in enumerate(self.emoji_buttons):
            frame = self.emoji_frames[i]
            
            if btn == selected_button:
                # 设置选中状态样式
                btn.setStyleSheet("background-color: #2a66c8; color: white; border: 2px solid #1a56b8;")
                frame.setStyleSheet("background-color: #e6f0ff; padding: 5px; border: 2px solid #4a86e8; border-radius: 4px;")
            else:
                # 恢复默认样式
                btn.setStyleSheet("")
                frame.setStyleSheet("background-color: white; padding: 5px;")

    def on_emoji_button_clicked(self):
        """处理表情按钮点击事件"""
        # 获取按钮的表情属性
        btn = self.sender()
        emoji_code = btn.property("emoji_code")
        emoji_name = btn.property("emoji_name")
        emoji_desc = btn.property("emoji_desc")
        
        # 更新选中的表情
        self.selected_emoji = {
            "code": emoji_code,
            "name": emoji_name,
            "description": emoji_desc
        }
        self.selected_emoji_label.setText(f"{emoji_name} ({emoji_desc}) - emoji_{emoji_code}")
        
        # 更新按钮选中状态视觉反馈
        self.current_selected_button = btn
        self.update_emoji_selection_style(btn)
        
        # 打开文件选择器
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            f"选择要作为{emoji_name}表情的图片",
            "",
            "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"
        )
        
        if file_path:
            # 获取文件扩展名
            extension = os.path.splitext(file_path)[1]
            
            # 构建新文件名
            emoji_filename = f"emoji_{emoji_code}{extension}"
            
            # 添加到文件列表
            self.add_file_to_table(file_path, emoji_filename, True)
            
            self.log(f"已选择图片并重命名为: {emoji_filename}")
    
    def add_file_to_table(self, file_path, display_name=None, selected=False):
        """添加文件到表格"""
        if display_name is None:
            display_name = os.path.basename(file_path)
        
        # 添加到内部数据结构
        self.file_list.append({
            "path": file_path,
            "display_name": display_name,
            "selected": selected
        })
        
        # 更新表格显示
        self.update_file_table()
    
    def update_file_table(self):
        """更新文件表格显示"""
        # 清空表格
        self.file_table.setRowCount(0)
        
        # 添加所有文件
        for i, file_item in enumerate(self.file_list):
            self.file_table.insertRow(i)
            
            # 创建选择复选框
            checkbox = QCheckBox()
            checkbox.setChecked(file_item["selected"])
            checkbox.stateChanged.connect(lambda state, row=i: self.on_checkbox_changed(state, row))
            
            # 创建单元格
            # 复选框放在中心位置
            self.file_table.setCellWidget(i, 0, self.center_widget(checkbox))
            self.file_table.setItem(i, 1, QTableWidgetItem(file_item["display_name"]))
    
    def center_widget(self, widget):
        """将小部件放在一个居中的容器中"""
        container = QWidget()
        layout = QHBoxLayout(container)
        layout.addWidget(widget)
        layout.setAlignment(Qt.AlignCenter)
        layout.setContentsMargins(0, 0, 0, 0)
        return container
    
    def on_checkbox_changed(self, state, row):
        """处理复选框状态变化"""
        if row < len(self.file_list):
            self.file_list[row]["selected"] = (state == Qt.Checked)
    
    def select_files(self):
        """选择要添加的文件"""
        files, _ = QFileDialog.getOpenFileNames(
            self,
            "选择图片文件",
            "",
            "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"
        )
        
        for file_path in files:
            self.add_file_to_table(file_path)
    
    def remove_selected(self):
        """移除选中的文件"""
        # 创建要保留的文件列表
        to_keep = [f for f in self.file_list if not f["selected"]]
        
        # 更新文件列表
        removed_count = len(self.file_list) - len(to_keep)
        self.file_list = to_keep
        self.update_file_table()
        
        if removed_count > 0:
            self.log(f"已移除 {removed_count} 个文件")
    
    def clear_files(self):
        """清空文件列表"""
        count = len(self.file_list)
        self.file_list.clear()
        self.update_file_table()
        
        if count > 0:
            self.log(f"已清空 {count} 个文件")
    
    def select_output_dir(self):
        """选择输出目录"""
        dir_path = QFileDialog.getExistingDirectory(
            self,
            "选择输出目录",
            self.output_dir
        )
        
        if dir_path:
            self.output_dir = dir_path
            self.output_dir_entry.setText(dir_path)
    
    def clear_log(self):
        """清空日志文本"""
        self.log_text.clear()
    
    def log(self, message):
        """向日志区域添加消息"""
        self.log_text.append(message)
    
    def show_help(self):
        """显示帮助信息"""
        msg_box = QMessageBox(self)
        msg_box.setWindowTitle("帮助")
        msg_box.setText(HELP_TEXT)
        msg_box.setStyleSheet("QLabel{min-width: 500px;}")
        msg_box.exec_()
    
    def start_conversion(self, convert_all):
        """开始转换图片"""
        # 获取要转换的文件
        input_files = []
        output_names = []
        
        for file_item in self.file_list:
            if convert_all or file_item["selected"]:
                input_files.append(file_item["path"])
                output_names.append(file_item["display_name"])
        
        if not input_files:
            msg = "没有找到可转换的文件" if convert_all else "没有选中任何文件"
            QMessageBox.warning(self, "警告", msg)
            return
        
        # 创建输出目录
        output_dir = self.output_dir_entry.text()
        os.makedirs(output_dir, exist_ok=True)
        
        # 解析转换参数
        resolution = self.res_combo.currentText()
        width, height = map(int, resolution.split('x'))
        
        # 设置压缩方式
        compress_method = self.compress_combo.currentText()
        if compress_method == "RLE":
            compress = CompressMethod.RLE
        elif compress_method == "LZ4":
            compress = CompressMethod.LZ4
        else:
            compress = CompressMethod.NONE
        
        # 执行转换
        self.convert_images(input_files, output_names, width, height, compress, output_dir)
    
    def convert_images(self, input_files, output_names, width, height, compress, output_dir):
        """转换图片为LVGL格式"""
        success_count = 0
        total_files = len(input_files)
        
        for idx, (file_path, output_name) in enumerate(zip(input_files, output_names)):
            try:
                self.log(f"正在处理: {os.path.basename(file_path)}")
                
                with Image.open(file_path) as img:
                    # 调整图片大小
                    img = img.resize((width, height), Image.Resampling.LANCZOS)
                    
                    # 处理颜色格式
                    color_format_str = self.color_combo.currentText()
                    if color_format_str == "自动识别":
                        # 检测透明通道
                        has_alpha = img.mode in ('RGBA', 'LA') or (img.mode == 'P' and 'transparency' in img.info)
                        if has_alpha:
                            img = img.convert('RGBA')
                            cf = ColorFormat.RGB565A8
                        else:
                            img = img.convert('RGB')
                            cf = ColorFormat.RGB565
                    else:
                        if color_format_str == "RGB565A8":
                            img = img.convert('RGBA')
                            cf = ColorFormat.RGB565A8
                        else:
                            img = img.convert('RGB')
                            cf = ColorFormat.RGB565
                    
                    # 使用重命名后的文件名（不含扩展名）
                    base_name = os.path.splitext(output_name)[0]
                    
                    # 保存调整后的图片
                    output_image_path = os.path.join(output_dir, f"{base_name}_{width}x{height}.png")
                    img.save(output_image_path, 'PNG')
                    
                    # 创建临时文件
                    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmpfile:
                        temp_path = tmpfile.name
                        img.save(temp_path, 'PNG')
                    
                    # 转换为LVGL C数组
                    lvgl_img = LVGLImage().from_png(temp_path, cf=cf)
                    output_c_path = os.path.join(output_dir, f"{base_name}.c")
                    lvgl_img.to_c_array(output_c_path, compress=compress)
                    
                    success_count += 1
                    os.unlink(temp_path)
                    self.log(f"成功转换: {base_name}.c\n")
                
            except Exception as e:
                self.log(f"转换失败: {str(e)}\n")
        
        self.log(f"转换完成! 成功 {success_count}/{total_files} 个文件\n")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    
    # 设置应用样式
    app.setStyle("Fusion")
    
    # 创建并显示主窗口
    window = ImageConverterApp()
    window.show()
    
    sys.exit(app.exec_())