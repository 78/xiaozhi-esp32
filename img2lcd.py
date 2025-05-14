import sys
import os
import tempfile
from PIL import Image
import cv2
import numpy as np
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                              QHBoxLayout, QPushButton, QLabel, QLineEdit, 
                              QFileDialog, QGroupBox, QFormLayout, QSpinBox, 
                              QTextEdit, QMessageBox, QStatusBar, QComboBox,
                              QListWidget, QProgressBar, QCheckBox, QScrollArea,
                              QGridLayout)
from PySide6.QtGui import QIcon, QFont, QPixmap
from PySide6.QtCore import Qt, QSize

class ImageConverter(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP32视频转换器")
        self.setMinimumSize(800, 600)
        
        # 设置中心部件
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.main_layout = QVBoxLayout(self.central_widget)
        
        # 存储输入和输出路径 - 移到这里，确保在创建UI之前初始化
        self.input_image_paths = []
        # 设置默认输出目录
        self.output_directory = "E:\\work\\xiaozhi-esp32\\main\\images\\doufu"
        # 确保目录存在
        os.makedirs(self.output_directory, exist_ok=True)
        self.temp_dir = None
        
        # 创建输入区域
        self.create_input_group()
        
        # 创建预览区域
        self.create_image_preview()
        
        # 创建输出区域 - 现在调用这个方法时self.output_directory已经存在
        self.create_output_group()
        
        # 创建格式设置区域
        self.create_format_group()
        
        # 创建日志区域
        self.create_preview_group()
        
        # 创建进度条
        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        self.main_layout.addWidget(self.progress_bar)
        
        # 创建转换按钮
        self.convert_button = QPushButton("转换视频为图片")
        self.convert_button.setFixedHeight(40)
        self.convert_button.setFont(QFont("Arial", 12, QFont.Bold))
        self.convert_button.clicked.connect(self.convert_images)
        self.main_layout.addWidget(self.convert_button)
        
        # 添加状态栏
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("准备就绪")

    def create_input_group(self):
        input_group = QGroupBox("输入设置")
        layout = QVBoxLayout()
        
        # 视频参数设置（固定值，不可修改）
        info_label = QLabel("注意: 视频将提取17帧，分辨率固定为240x240")
        info_label.setStyleSheet("color: blue;")
        layout.addWidget(info_label)
        
        # 输入文件选择
        input_file_layout = QHBoxLayout()
        self.browse_video_button = QPushButton("选择视频文件...")
        self.browse_video_button.clicked.connect(self.select_input_video)
        self.clear_button = QPushButton("清空列表")
        self.clear_button.clicked.connect(self.clear_input_list)
        
        input_file_layout.addWidget(self.browse_video_button)
        input_file_layout.addWidget(self.clear_button)
        layout.addLayout(input_file_layout)
        
        # 文件列表
        self.file_list = QListWidget()
        self.file_list.setMinimumHeight(100)
        layout.addWidget(QLabel("已选择的文件:"))
        layout.addWidget(self.file_list)
        
        input_group.setLayout(layout)
        self.main_layout.addWidget(input_group)
    
    def select_input_video(self):
        file_path, _ = QFileDialog.getOpenFileName(
            self, "选择视频文件", "", "视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv)"
        )
        if file_path:
            self.clear_input_list()  # 清除之前的所有文件
            
            # 创建临时目录存储帧
            if self.temp_dir:
                # 清理旧的临时目录
                import shutil
                try:
                    shutil.rmtree(self.temp_dir)
                except:
                    pass
            
            self.temp_dir = tempfile.mkdtemp()
            
            # 提取视频帧（固定17帧）
            self.extract_frames(file_path, 17)
            
            # 更新状态
            self.status_bar.showMessage(f"已从视频提取 {len(self.input_image_paths)} 个帧")
    
    def extract_frames(self, video_path, frame_count):
        try:
            # 打开视频文件
            cap = cv2.VideoCapture(video_path)
            if not cap.isOpened():
                QMessageBox.critical(self, "错误", "无法打开视频文件")
                return
            
            # 获取视频信息
            total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
            fps = cap.get(cv2.CAP_PROP_FPS)
            duration = total_frames / fps
            width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
            
            self.log_text.append(f"视频信息:")
            self.log_text.append(f"  - 原始分辨率: {width}x{height}")
            self.log_text.append(f"  - 总帧数: {total_frames}")
            self.log_text.append(f"  - FPS: {fps:.2f}")
            self.log_text.append(f"  - 持续时间: {duration:.2f}秒")
            self.log_text.append(f"  - 输出分辨率: 240x240（固定）")
            
            # 计算需要提取的帧的间隔
            if frame_count > total_frames:
                frame_count = total_frames
                QMessageBox.warning(self, "警告", f"视频总帧数为{total_frames}，小于请求的帧数。将提取所有帧。")
            
            interval = total_frames / frame_count
            
            # 目标分辨率（固定为240x240）
            target_width = 240
            target_height = 240
            
            # 提取帧
            frames = []
            for i in range(frame_count):
                # 计算当前帧位置
                frame_pos = int(i * interval)
                cap.set(cv2.CAP_PROP_POS_FRAMES, frame_pos)
                
                ret, frame = cap.read()
                if ret:
                    # 调整分辨率为240x240
                    frame = cv2.resize(frame, (target_width, target_height), interpolation=cv2.INTER_AREA)
                    
                    # 转换BGR到RGB（OpenCV是BGR，PIL是RGB）
                    frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                    
                    # 保存帧到临时文件
                    frame_path = os.path.join(self.temp_dir, f"frame_{i:04d}.png")
                    img_pil = Image.fromarray(frame_rgb)
                    img_pil.save(frame_path)
                    
                    self.input_image_paths.append(frame_path)
                    
                    # 更新UI
                    self.progress_bar.setValue(int((i+1) / frame_count * 100))
                    QApplication.processEvents()
            
            cap.release()
            self.update_file_list()
            self.update_preview_images()  # 更新预览图
            self.log_text.append(f"成功提取 {len(self.input_image_paths)} 帧\n")
            
        except Exception as e:
            QMessageBox.critical(self, "错误", f"提取视频帧时出错: {str(e)}")
            import traceback
            self.log_text.append(traceback.format_exc())
    
    def create_image_preview(self):
        preview_group = QGroupBox("图片预览")
        layout = QVBoxLayout()
        
        # 创建预览区
        preview_scroll = QScrollArea()
        preview_scroll.setWidgetResizable(True)
        preview_container = QWidget()
        self.preview_grid = QGridLayout(preview_container)
        preview_scroll.setWidget(preview_container)
        
        # 设置预览区高度
        preview_scroll.setMinimumHeight(200)
        
        layout.addWidget(preview_scroll)
        preview_group.setLayout(layout)
        self.main_layout.addWidget(preview_group)
        
    def update_preview_images(self):
        # 清除现有的预览图
        for i in reversed(range(self.preview_grid.count())):
            self.preview_grid.itemAt(i).widget().setParent(None)
        
        # 添加新的预览图
        for i, img_path in enumerate(self.input_image_paths):
            try:
                pixmap = QPixmap(img_path)
                if not pixmap.isNull():
                    # 创建缩略图 (限制大小为100x100)
                    thumb = pixmap.scaled(100, 100, Qt.KeepAspectRatio, Qt.SmoothTransformation)
                    
                    # 创建标签显示缩略图
                    img_label = QLabel()
                    img_label.setPixmap(thumb)
                    img_label.setAlignment(Qt.AlignCenter)
                    img_label.setToolTip(f"第 {i+1} 帧")
                    
                    # 显示帧号
                    frame_label = QLabel(f"第 {i+1} 帧")
                    frame_label.setAlignment(Qt.AlignCenter)
                    
                    # 创建容器来放置图片和标签
                    container = QWidget()
                    container_layout = QVBoxLayout(container)
                    container_layout.addWidget(img_label)
                    container_layout.addWidget(frame_label)
                    
                    # 添加到网格布局
                    row = i // 5
                    col = i % 5
                    self.preview_grid.addWidget(container, row, col)
            except Exception as e:
                print(f"加载图片预览失败: {str(e)}")
    
    def create_output_group(self):
        output_group = QGroupBox("输出设置")
        layout = QFormLayout()
        
        # 输出目录路径
        self.output_layout = QHBoxLayout()
        self.output_path = QLineEdit()
        self.output_path.setPlaceholderText("选择输出目录...")
        self.output_path.setText(self.output_directory)  # 设置默认目录
        self.output_browse = QPushButton("浏览...")
        self.output_browse.clicked.connect(self.select_output_directory)
        
        self.output_layout.addWidget(self.output_path)
        self.output_layout.addWidget(self.output_browse)
        
        layout.addRow("输出目录:", self.output_layout)
        
        # 数组名称前缀（固定为output_）
        prefix_label = QLabel("output_")
        prefix_label.setStyleSheet("color: gray;")
        layout.addRow("数组名称前缀:", prefix_label)
        
        # 起始编号（固定为1）
        start_number_label = QLabel("1")
        start_number_label.setStyleSheet("color: gray;")
        layout.addRow("起始编号:", start_number_label)
        
        # 是否在同一目录下
        self.same_directory = QCheckBox("在原图片目录输出")
        self.same_directory.setChecked(False)
        self.same_directory.toggled.connect(self.toggle_output_directory)
        layout.addRow("", self.same_directory)
        
        output_group.setLayout(layout)
        self.main_layout.addWidget(output_group)

    def create_format_group(self):
        format_group = QGroupBox("格式设置（固定值）")
        layout = QFormLayout()
        
        # 颜色格式（固定为RGB565）
        color_format_label = QLabel("RGB565")
        color_format_label.setStyleSheet("color: gray;")
        layout.addRow("颜色格式:", color_format_label)
        
        # 每行像素数（固定为8）
        pixels_per_line_label = QLabel("8")
        pixels_per_line_label.setStyleSheet("color: gray;")
        layout.addRow("每行像素数:", pixels_per_line_label)
        
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
    
    def clear_input_list(self):
        self.input_image_paths = []
        self.file_list.clear()
        self.status_bar.showMessage("已清空文件列表")
    
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
            QMessageBox.warning(self, "警告", "请先选择视频文件！")
            return
            
        if not self.same_directory.isChecked() and not self.output_path.text():
            QMessageBox.warning(self, "警告", "请设置输出目录或选择在原图片目录输出！")
            return
        
        # 固定设置
        prefix = "output_"
        start_number = 1
        pixels_per_line = 8
        
        # 清空日志
        self.log_text.append("开始转换...")
        
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
    
    def closeEvent(self, event):
        # 清理临时目录
        if self.temp_dir and os.path.exists(self.temp_dir):
            import shutil
            try:
                shutil.rmtree(self.temp_dir)
            except:
                pass
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = ImageConverter()
    window.show()
    sys.exit(app.exec())
