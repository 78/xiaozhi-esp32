import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from PIL import Image
import os
import tempfile
import sys
from LVGLImage import LVGLImage, ColorFormat, CompressMethod

HELP_TEXT = """LVGL图片转换工具使用说明：

1. 添加文件：点击“添加文件”按钮选择需要转换的图片，支持批量导入

2. 移除文件：在列表中选中文件前的复选框“[ ]”（选中后会变成“[√]”），点击“移除选中”可删除选定文件

3. 设置分辨率：选择需要的分辨率，如128x128
   建议根据自己的设备的屏幕分辨率来选择。过大和过小都会影响显示效果。

4. 颜色格式：选择“自动识别”会根据图片是否透明自动选择，或手动指定
   除非你了解这个选项，否则建议使用自动识别，不然可能会出现一些意想不到的问题……

5. 压缩方式：选择NONE或RLE压缩
   除非你了解这个选项，否则建议保持默认NONE不压缩

6. 输出目录：设置转换后文件的保存路径
   默认为程序所在目录下的output文件夹

7. 转换：点击“转换全部”或“转换选中”开始转换
"""

class ImageConverterApp:
    def __init__(self, root):
        self.root = root
        self.root.title("LVGL图片转换工具")
        self.root.geometry("750x650")
        
        # 初始化变量
        self.output_dir = tk.StringVar(value=os.path.abspath("output"))
        self.resolution = tk.StringVar(value="128x128")
        self.color_format = tk.StringVar(value="自动识别")
        self.compress_method = tk.StringVar(value="NONE")

        # 创建UI组件
        self.create_widgets()
        self.redirect_output()

    def create_widgets(self):
        # 参数设置框架
        settings_frame = ttk.LabelFrame(self.root, text="转换设置")
        settings_frame.grid(row=0, column=0, padx=10, pady=5, sticky="ew")

        # 分辨率设置
        ttk.Label(settings_frame, text="分辨率:").grid(row=0, column=0, padx=2)
        ttk.Combobox(settings_frame, textvariable=self.resolution, 
                    values=["512x512", "256x256", "128x128", "64x64", "32x32"], width=8).grid(row=0, column=1, padx=2)

        # 颜色格式
        ttk.Label(settings_frame, text="颜色格式:").grid(row=0, column=2, padx=2)
        ttk.Combobox(settings_frame, textvariable=self.color_format,
                    values=["自动识别", "RGB565", "RGB565A8"], width=10).grid(row=0, column=3, padx=2)

        # 压缩方式
        ttk.Label(settings_frame, text="压缩方式:").grid(row=0, column=4, padx=2)
        ttk.Combobox(settings_frame, textvariable=self.compress_method,
                    values=["NONE", "RLE"], width=8).grid(row=0, column=5, padx=2)

        # 文件操作框架
        file_frame = ttk.LabelFrame(self.root, text="选取文件")
        file_frame.grid(row=1, column=0, padx=10, pady=5, sticky="nsew")

        # 文件操作按钮
        btn_frame = ttk.Frame(file_frame)
        btn_frame.pack(fill=tk.X, pady=2)
        ttk.Button(btn_frame, text="添加文件", command=self.select_files).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_frame, text="移除选中", command=self.remove_selected).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_frame, text="清空列表", command=self.clear_files).pack(side=tk.LEFT, padx=2)

        # 文件列表（Treeview）
        self.tree = ttk.Treeview(file_frame, columns=("selected", "filename"), 
                               show="headings", height=10)
        self.tree.heading("selected", text="选择", anchor=tk.W)
        self.tree.heading("filename", text="文件名", anchor=tk.W)
        self.tree.column("selected", width=60, anchor=tk.W)
        self.tree.column("filename", width=600, anchor=tk.W)
        self.tree.pack(fill=tk.BOTH, expand=True)
        self.tree.bind("<ButtonRelease-1>", self.on_tree_click)

        # 输出目录
        output_frame = ttk.LabelFrame(self.root, text="输出目录")
        output_frame.grid(row=2, column=0, padx=10, pady=5, sticky="ew")
        ttk.Entry(output_frame, textvariable=self.output_dir, width=60).pack(side=tk.LEFT, padx=5)
        ttk.Button(output_frame, text="浏览", command=self.select_output_dir).pack(side=tk.RIGHT, padx=5)

        # 转换按钮和帮助按钮
        convert_frame = ttk.Frame(self.root)
        convert_frame.grid(row=3, column=0, padx=10, pady=10)
        ttk.Button(convert_frame, text="转换全部文件", command=lambda: self.start_conversion(True)).pack(side=tk.LEFT, padx=5)
        ttk.Button(convert_frame, text="转换选中文件", command=lambda: self.start_conversion(False)).pack(side=tk.LEFT, padx=5)
        ttk.Button(convert_frame, text="帮助", command=self.show_help).pack(side=tk.RIGHT, padx=5)

        # 日志区域（新增清空按钮部分）
        log_frame = ttk.LabelFrame(self.root, text="日志")
        log_frame.grid(row=4, column=0, padx=10, pady=5, sticky="nsew")
        
        # 添加按钮框架
        log_btn_frame = ttk.Frame(log_frame)
        log_btn_frame.pack(fill=tk.X, side=tk.BOTTOM)
        
        # 清空日志按钮
        ttk.Button(log_btn_frame, text="清空日志", command=self.clear_log).pack(side=tk.RIGHT, padx=5, pady=2)
        
        self.log_text = tk.Text(log_frame, height=15)
        self.log_text.pack(fill=tk.BOTH, expand=True)

        # 布局配置
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(1, weight=1)
        self.root.rowconfigure(4, weight=1)

    def clear_log(self):
        """清空日志内容"""
        self.log_text.delete(1.0, tk.END)

    def show_help(self):
        messagebox.showinfo("帮助", HELP_TEXT)

    def redirect_output(self):
        class StdoutRedirector:
            def __init__(self, text_widget):
                self.text_widget = text_widget
                self.original_stdout = sys.stdout

            def write(self, message):
                self.text_widget.insert(tk.END, message)
                self.text_widget.see(tk.END)
                self.original_stdout.write(message)

            def flush(self):
                self.original_stdout.flush()

        sys.stdout = StdoutRedirector(self.log_text)

    def on_tree_click(self, event):
        region = self.tree.identify("region", event.x, event.y)
        if region == "cell":
            col = self.tree.identify_column(event.x)
            item = self.tree.identify_row(event.y)
            if col == "#1":  # 点击的是选中列
                current_val = self.tree.item(item, "values")[0]
                new_val = "[√]" if current_val == "[ ]" else "[ ]"
                self.tree.item(item, values=(new_val, self.tree.item(item, "values")[1]))

    def select_output_dir(self):
        path = filedialog.askdirectory()
        if path:
            self.output_dir.set(path)

    def select_files(self):
        files = filedialog.askopenfilenames(filetypes=[("图片文件", "*.png;*.jpg;*.jpeg;*.bmp;*.gif")])
        for f in files:
            self.tree.insert("", tk.END, values=("[ ]", os.path.basename(f)), tags=(f,))

    def remove_selected(self):
        to_remove = []
        for item in self.tree.get_children():
            if self.tree.item(item, "values")[0] == "[√]":
                to_remove.append(item)
        for item in reversed(to_remove):
            self.tree.delete(item)

    def clear_files(self):
        for item in self.tree.get_children():
            self.tree.delete(item)

    def start_conversion(self, convert_all):
        input_files = [
            self.tree.item(item, "tags")[0]
            for item in self.tree.get_children()
            if convert_all or self.tree.item(item, "values")[0] == "[√]"
        ]
        
        if not input_files:
            msg = "没有找到可转换的文件" if convert_all else "没有选中任何文件"
            messagebox.showwarning("警告", msg)
            return
        
        os.makedirs(self.output_dir.get(), exist_ok=True)
        
        # 解析转换参数
        width, height = map(int, self.resolution.get().split('x'))
        compress = CompressMethod.RLE if self.compress_method.get() == "RLE" else CompressMethod.NONE

        # 执行转换
        self.convert_images(input_files, width, height, compress)

    def convert_images(self, input_files, width, height, compress):
        success_count = 0
        total_files = len(input_files)
        
        for idx, file_path in enumerate(input_files):
            try:
                print(f"正在处理: {os.path.basename(file_path)}")
                
                with Image.open(file_path) as img:
                    # 调整图片大小
                    img = img.resize((width, height), Image.Resampling.LANCZOS)
                    
                    # 处理颜色格式
                    color_format_str = self.color_format.get()
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

                    # 保存调整后的图片
                    base_name = os.path.splitext(os.path.basename(file_path))[0]
                    output_image_path = os.path.join(self.output_dir.get(), f"{base_name}_{width}x{height}.png")
                    img.save(output_image_path, 'PNG')

                    # 创建临时文件
                    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmpfile:
                        temp_path = tmpfile.name
                        img.save(temp_path, 'PNG')

                    # 转换为LVGL C数组
                    lvgl_img = LVGLImage().from_png(temp_path, cf=cf)
                    output_c_path = os.path.join(self.output_dir.get(), f"{base_name}.c")
                    lvgl_img.to_c_array(output_c_path, compress=compress)

                    success_count += 1
                    os.unlink(temp_path)
                    print(f"成功转换: {base_name}.c\n")

            except Exception as e:
                print(f"转换失败: {str(e)}\n")

        print(f"转换完成! 成功 {success_count}/{total_files} 个文件\n")

if __name__ == "__main__":
    root = tk.Tk()
    app = ImageConverterApp(root)
    root.mainloop()
