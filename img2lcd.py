import sys
import os
from PIL import Image
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                              QHBoxLayout, QPushButton, QLabel, QLineEdit, 
                              QFileDialog, QGroupBox, QFormLayout, QSpinBox, 
                              QTextEdit, QMessageBox, QStatusBar, QComboBox,
                              QListWidget, QProgressBar, QCheckBox)
from PySide6.QtGui import QIcon, QFont
from PySide6.QtCore import Qt

class ImageConverter(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP32批量图片转换器")
        self.setMinimumSize(800, 600)
        
        # 设置中心部件
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.main_layout = QVBoxLayout(self.central_widget)
        
        # 创建输入区域
        self.create_input_group()
        
        # 创建输出区域
        self.create_output_group()
        
        # 创建格式设置区域
        self.create_format_group()
        
        # 创建预览区域
        self.create_preview_group()
        
        # 创建进度条
        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        self.main_layout.addWidget(self.progress_bar)
        
        # 创建转换按钮
        self.convert_button = QPushButton("批量转换图片")
        self.convert_button.setFixedHeight(40)
        self.convert_button.setFont(QFont("Arial", 12, QFont.Bold))
        self.convert_button.clicked.connect(self.convert_images)
        self.main_layout.addWidget(self.convert_button)
        
        # 添加状态栏
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("准备就绪")

        # 存储输入和输出路径
        self.input_image_paths = []
        self.output_directory = ""
        
    def create_input_group(self):
        input_group = QGroupBox("输入设置")
        layout = QVBoxLayout()
        
        # 输入图片选择
        input_file_layout = QHBoxLayout()
        self.browse_button = QPushButton("选择多个图片...")
        self.browse_button.clicked.connect(self.select_input_images)
        self.clear_button = QPushButton("清空列表")
        self.clear_button.clicked.connect(self.clear_input_list)
        
        input_file_layout.addWidget(self.browse_button)
        input_file_layout.addWidget(self.clear_button)
        layout.addLayout(input_file_layout)
        
        # 文件列表
        self.file_list = QListWidget()
        self.file_list.setMinimumHeight(100)
        layout.addWidget(QLabel("已选择的图片:"))
        layout.addWidget(self.file_list)
        
        input_group.setLayout(layout)
        self.main_layout.addWidget(input_group)
    
    def create_output_group(self):
        output_group = QGroupBox("输出设置")
        layout = QFormLayout()
        
        # 输出目录路径
        self.output_layout = QHBoxLayout()
        self.output_path = QLineEdit()
        self.output_path.setPlaceholderText("选择输出目录...")
        self.output_browse = QPushButton("浏览...")
        self.output_browse.clicked.connect(self.select_output_directory)
        
        self.output_layout.addWidget(self.output_path)
        self.output_layout.addWidget(self.output_browse)
        
        layout.addRow("输出目录:", self.output_layout)
        
        # 数组名称前缀
        self.array_prefix = QLineEdit("output_")
        layout.addRow("数组名称前缀:", self.array_prefix)
        
        # 起始编号
        self.start_number = QSpinBox()
        self.start_number.setRange(1, 9999)
        self.start_number.setValue(1)
        layout.addRow("起始编号:", self.start_number)
        
        # 是否在同一目录下
        self.same_directory = QCheckBox("在原图片目录输出")
        self.same_directory.setChecked(False)
        self.same_directory.toggled.connect(self.toggle_output_directory)
        layout.addRow("", self.same_directory)
        
        output_group.setLayout(layout)
        self.main_layout.addWidget(output_group)

    def create_format_group(self):
        format_group = QGroupBox("格式设置")
        layout = QFormLayout()
        
        # 颜色格式
        self.color_format = QComboBox()
        self.color_format.addItems(["RGB565"])
        layout.addRow("颜色格式:", self.color_format)
        
        # 每行像素数
        self.pixels_per_line = QSpinBox()
        self.pixels_per_line.setRange(1, 32)
        self.pixels_per_line.setValue(8)
        layout.addRow("每行像素数:", self.pixels_per_line)
        
        format_group.setLayout(layout)
        self.main_layout.addWidget(format_group)
    
    def create_preview_group(self):
        preview_group = QGroupBox("转换日志")
        layout = QVBoxLayout()
        
        # 转换日志
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setPlaceholderText("转换日志将显示在这里...")
        layout.addWidget(self.log_text)
        
        preview_group.setLayout(layout)
        self.main_layout.addWidget(preview_group)
    
    def select_input_images(self):
        file_paths, _ = QFileDialog.getOpenFileNames(
            self, "选择多个图片", "", "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"
        )
        if file_paths:
            self.input_image_paths.extend(file_paths)
            self.update_file_list()
            
            # 更新状态
            self.status_bar.showMessage(f"已选择 {len(file_paths)} 个新图片，总计 {len(self.input_image_paths)} 个")
    
    def clear_input_list(self):
        self.input_image_paths = []
        self.file_list.clear()
        self.status_bar.showMessage("已清空图片列表")
    
    def update_file_list(self):
        self.file_list.clear()
        for path in self.input_image_paths:
            self.file_list.addItem(os.path.basename(path))
    
    def select_output_directory(self):
        directory = QFileDialog.getExistingDirectory(self, "选择输出目录")
        if directory:
            self.output_directory = directory
            self.output_path.setText(directory)
            self.status_bar.showMessage(f"输出将保存到: {directory}")
    
    def toggle_output_directory(self, checked):
        self.output_path.setEnabled(not checked)
        self.output_browse.setEnabled(not checked)
    
    def convert_images(self):
        # 检查输入
        if not self.input_image_paths:
            QMessageBox.warning(self, "警告", "请先选择输入图片！")
            return
            
        if not self.same_directory.isChecked() and not self.output_path.text():
            QMessageBox.warning(self, "警告", "请设置输出目录或选择在原图片目录输出！")
            return
            
        if not self.array_prefix.text():
            QMessageBox.warning(self, "警告", "请设置数组名称前缀！")
            return
        
        # 获取设置
        prefix = self.array_prefix.text()
        start_number = self.start_number.value()
        pixels_per_line = self.pixels_per_line.value()
        
        # 清空日志
        self.log_text.clear()
        
        # 开始批量处理
        total_images = len(self.input_image_paths)
        for i, image_path in enumerate(self.input_image_paths):
            try:
                # 更新进度
                progress = int((i / total_images) * 100)
                self.progress_bar.setValue(progress)
                self.status_bar.showMessage(f"正在处理 {i+1}/{total_images}: {os.path.basename(image_path)}")
                QApplication.processEvents()  # 让UI保持响应
                
                # 生成序列化的数字字符串
                seq_number = f"{start_number + i:04d}"
                
                # 设置输出路径
                if self.same_directory.isChecked():
                    output_dir = os.path.dirname(image_path)
                else:
                    output_dir = self.output_path.text()
                
                # 确保输出目录存在
                os.makedirs(output_dir, exist_ok=True)
                
                # 设置输出文件名和数组名
                file_name = f"{prefix}{seq_number}"
                output_path = os.path.join(output_dir, f"{file_name}.h")
                array_name = f"gImage_{file_name}"
                
                # 打开图像
                img = Image.open(image_path)
                width, height = img.size
                
                # 记录日志
                self.log_text.append(f"正在处理: {os.path.basename(image_path)}")
                self.log_text.append(f"  - 尺寸: {width}x{height}")
                self.log_text.append(f"  - 输出: {output_path}")
                self.log_text.append(f"  - 数组: {array_name}")
                
                # 打开输出文件
                with open(output_path, 'w') as f:
                    # 写入头部
                    f.write(f"const unsigned char {array_name}[{width * height * 2}] = {{ /* 0X10,0X10,0X00,0X{width:02X},0X00,0X{height:02X},0X01,0X1B, */\n")
                    
                    # 转换每个像素
                    line_count = 0
                    pixel_count = 0
                    line_buffer = ""
                    
                    for y in range(height):
                        for x in range(width):
                            pixel = img.getpixel((x, y))
                            
                            # 将RGB转换为RGB565格式
                            if img.mode == "RGBA":
                                r, g, b, a = pixel
                            elif img.mode == "RGB":
                                r, g, b = pixel
                            else:
                                img_rgb = img.convert('RGB')
                                r, g, b = img_rgb.getpixel((x, y))
                            
                            # RGB565: 5位R, 6位G, 5位B
                            r = (r >> 3) & 0x1F  # 5位
                            g = (g >> 2) & 0x3F  # 6位
                            b = (b >> 3) & 0x1F  # 5位
                            
                            rgb565 = (r << 11) | (g << 5) | b
                            high_byte = (rgb565 >> 8) & 0xFF
                            low_byte = rgb565 & 0xFF
                            
                            line_buffer += f"0X{high_byte:02X},0X{low_byte:02X},"
                            pixel_count += 1
                            
                            if pixel_count % pixels_per_line == 0 or (x == width - 1 and y == height - 1):
                                f.write(line_buffer + "\n")
                                line_buffer = ""
                                line_count += 1
                    
                    # 结束数组
                    f.write("};\n")
                
                self.log_text.append(f"  - 状态: 成功\n")
                
            except Exception as e:
                self.log_text.append(f"  - 状态: 失败 - {str(e)}\n")
        
        # 完成处理
        self.progress_bar.setValue(100)
        self.status_bar.showMessage(f"批量转换完成! 共处理 {total_images} 个图片")
        QMessageBox.information(self, "转换完成", f"已成功转换 {total_images} 个图片！")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = ImageConverter()
    window.show()
    sys.exit(app.exec())
