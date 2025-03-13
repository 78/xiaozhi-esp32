import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import os
import threading
import sys
from convert_audio_to_p3 import encode_audio_to_opus
from convert_p3_to_audio import decode_p3_to_audio

class AudioConverterApp:
    def __init__(self, master):
        self.master = master
        master.title("音频/P3 批量转换工具")
        master.geometry("680x600")  # 调整窗口高度

        # 初始化变量
        self.mode = tk.StringVar(value="audio_to_p3")
        self.output_dir = tk.StringVar()
        self.output_dir.set(os.path.abspath("output"))
        self.enable_loudnorm = tk.BooleanVar(value=True)
        self.target_lufs = tk.DoubleVar(value=-16.0)

        # 创建UI组件
        self.create_widgets()
        self.redirect_output()

    def create_widgets(self):
        # 模式选择
        mode_frame = ttk.LabelFrame(self.master, text="转换模式")
        mode_frame.grid(row=0, column=0, padx=10, pady=5, sticky="ew")
        
        ttk.Radiobutton(mode_frame, text="音频转P3", variable=self.mode,
                        value="audio_to_p3", command=self.toggle_settings,
                        width=12).grid(row=0, column=0, padx=5)
        ttk.Radiobutton(mode_frame, text="P3转音频", variable=self.mode,
                        value="p3_to_audio", command=self.toggle_settings,
                        width=12).grid(row=0, column=1, padx=5)

        # 响度设置
        self.loudnorm_frame = ttk.Frame(self.master)
        self.loudnorm_frame.grid(row=1, column=0, padx=10, pady=5, sticky="ew")
        
        ttk.Checkbutton(self.loudnorm_frame, text="启用响度调整", 
                       variable=self.enable_loudnorm, width=15
                       ).grid(row=0, column=0, padx=2)
        ttk.Entry(self.loudnorm_frame, textvariable=self.target_lufs, 
                 width=6).grid(row=0, column=1, padx=2)
        ttk.Label(self.loudnorm_frame, text="LUFS").grid(row=0, column=2, padx=2)

        # 文件选择
        file_frame = ttk.LabelFrame(self.master, text="输入文件")
        file_frame.grid(row=2, column=0, padx=10, pady=5, sticky="nsew")
        
        # 文件操作按钮
        ttk.Button(file_frame, text="选择文件", command=self.select_files,
                  width=12).grid(row=0, column=0, padx=5, pady=2)
        ttk.Button(file_frame, text="移除选中", command=self.remove_selected,
                  width=12).grid(row=0, column=1, padx=5, pady=2)
        ttk.Button(file_frame, text="清空列表", command=self.clear_files,
                  width=12).grid(row=0, column=2, padx=5, pady=2)

        # 文件列表（使用Treeview）
        self.tree = ttk.Treeview(file_frame, columns=("selected", "filename"), 
                               show="headings", height=8)
        self.tree.heading("selected", text="选中", anchor=tk.W)
        self.tree.heading("filename", text="文件名", anchor=tk.W)
        self.tree.column("selected", width=60, anchor=tk.W)
        self.tree.column("filename", width=600, anchor=tk.W)
        self.tree.grid(row=1, column=0, columnspan=3, sticky="nsew", padx=5, pady=2)
        self.tree.bind("<ButtonRelease-1>", self.on_tree_click)

        # 输出目录
        output_frame = ttk.LabelFrame(self.master, text="输出目录")
        output_frame.grid(row=3, column=0, padx=10, pady=5, sticky="ew")
        
        ttk.Entry(output_frame, textvariable=self.output_dir, width=60
                 ).grid(row=0, column=0, padx=5, sticky="ew")
        ttk.Button(output_frame, text="浏览", command=self.select_output_dir,
                  width=8).grid(row=0, column=1, padx=5)

        # 转换按钮区域
        button_frame = ttk.Frame(self.master)
        button_frame.grid(row=4, column=0, padx=10, pady=10, sticky="ew")
        
        ttk.Button(button_frame, text="转换全部文件", command=lambda: self.start_conversion(True),
                  width=15).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="转换选中文件", command=lambda: self.start_conversion(False),
                  width=15).pack(side=tk.LEFT, padx=5)

        # 日志区域
        log_frame = ttk.LabelFrame(self.master, text="日志")
        log_frame.grid(row=5, column=0, padx=10, pady=5, sticky="nsew")
        
        self.log_text = tk.Text(log_frame, height=14, width=80)
        self.log_text.pack(fill=tk.BOTH, expand=True)

        # 配置布局权重
        self.master.columnconfigure(0, weight=1)
        self.master.rowconfigure(2, weight=1)
        self.master.rowconfigure(5, weight=3)
        file_frame.columnconfigure(0, weight=1)
        file_frame.rowconfigure(1, weight=1)

    def toggle_settings(self):
        if self.mode.get() == "audio_to_p3":
            self.loudnorm_frame.grid()
        else:
            self.loudnorm_frame.grid_remove()

    def select_files(self):
        file_types = [
            ("音频文件", "*.wav *.mp3 *.ogg *.flac") if self.mode.get() == "audio_to_p3" 
            else ("P3文件", "*.p3")
        ]
        
        files = filedialog.askopenfilenames(filetypes=file_types)
        for f in files:
            self.tree.insert("", tk.END, values=("[ ]", os.path.basename(f)), tags=(f,))

    def on_tree_click(self, event):
        """处理复选框点击事件"""
        region = self.tree.identify("region", event.x, event.y)
        if region == "cell":
            col = self.tree.identify_column(event.x)
            item = self.tree.identify_row(event.y)
            if col == "#1":  # 点击的是选中列
                current_val = self.tree.item(item, "values")[0]
                new_val = "[√]" if current_val == "[ ]" else "[ ]"
                self.tree.item(item, values=(new_val, self.tree.item(item, "values")[1]))

    def remove_selected(self):
        """移除选中的文件"""
        to_remove = []
        for item in self.tree.get_children():
            if self.tree.item(item, "values")[0] == "[√]":
                to_remove.append(item)
        for item in reversed(to_remove):
            self.tree.delete(item)

    def clear_files(self):
        """清空所有文件"""
        for item in self.tree.get_children():
            self.tree.delete(item)

    def select_output_dir(self):
        path = filedialog.askdirectory()
        if path:
            self.output_dir.set(path)

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

    def start_conversion(self, convert_all):
        """开始转换"""
        input_files = []
        for item in self.tree.get_children():
            if convert_all or self.tree.item(item, "values")[0] == "[√]":
                input_files.append(self.tree.item(item, "tags")[0])
        
        if not input_files:
            msg = "没有找到可转换的文件" if convert_all else "没有选中任何文件"
            messagebox.showwarning("警告", msg)
            return
        
        os.makedirs(self.output_dir.get(), exist_ok=True)

        try:
            if self.mode.get() == "audio_to_p3":
                target_lufs = self.target_lufs.get() if self.enable_loudnorm.get() else None
                thread = threading.Thread(target=self.convert_audio_to_p3, args=(target_lufs, input_files))
            else:
                thread = threading.Thread(target=self.convert_p3_to_audio, args=(input_files,))
            
            thread.start()
        except Exception as e:
            print(f"转换初始化失败: {str(e)}")

    def convert_audio_to_p3(self, target_lufs, input_files):
        """音频转P3转换逻辑"""
        for input_path in input_files:
            try:
                filename = os.path.basename(input_path)
                base_name = os.path.splitext(filename)[0]
                output_path = os.path.join(self.output_dir.get(), f"{base_name}.p3")
                
                print(f"正在转换: {filename}")
                encode_audio_to_opus(input_path, output_path, target_lufs)
                print(f"转换成功: {filename}\n")
            except Exception as e:
                print(f"转换失败: {str(e)}\n")

    def convert_p3_to_audio(self, input_files):
        """P3转音频转换逻辑"""
        for input_path in input_files:
            try:
                filename = os.path.basename(input_path)
                base_name = os.path.splitext(filename)[0]
                output_path = os.path.join(self.output_dir.get(), f"{base_name}.wav")
                
                print(f"正在转换: {filename}")
                decode_p3_to_audio(input_path, output_path)
                print(f"转换成功: {filename}\n")
            except Exception as e:
                print(f"转换失败: {str(e)}\n")

if __name__ == "__main__":
    root = tk.Tk()
    app = AudioConverterApp(root)
    root.mainloop()