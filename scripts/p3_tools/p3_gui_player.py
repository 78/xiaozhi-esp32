import tkinter as tk
from tkinter import filedialog, messagebox
import threading
import time
import opuslib
import struct
import numpy as np
import sounddevice as sd
import os


def play_p3_file(input_file, stop_event=None, pause_event=None):
    """
    播放p3格式的音频文件
    p3格式: [1字节类型, 1字节保留, 2字节长度, Opus数据]
    """
    # 初始化Opus解码器
    sample_rate = 16000  # 采样率固定为16000Hz
    channels = 1  # 单声道
    decoder = opuslib.Decoder(sample_rate, channels)
    
    # 帧大小 (60ms)
    frame_size = int(sample_rate * 60 / 1000)
    
    # 打开音频流
    stream = sd.OutputStream(
        samplerate=sample_rate,
        channels=channels,
        dtype='int16'
    )
    stream.start()
    
    try:
        with open(input_file, 'rb') as f:
            print(f"正在播放: {input_file}")
            
            while True:
                if stop_event and stop_event.is_set():
                    break

                if pause_event and pause_event.is_set():
                    time.sleep(0.1)
                    continue

                # 读取头部 (4字节)
                header = f.read(4)
                if not header or len(header) < 4:
                    break
                
                # 解析头部
                packet_type, reserved, data_len = struct.unpack('>BBH', header)
                
                # 读取Opus数据
                opus_data = f.read(data_len)
                if not opus_data or len(opus_data) < data_len:
                    break
                
                # 解码Opus数据
                pcm_data = decoder.decode(opus_data, frame_size)
                
                # 将字节转换为numpy数组
                audio_array = np.frombuffer(pcm_data, dtype=np.int16)
                
                # 播放音频
                stream.write(audio_array)
                
    except KeyboardInterrupt:
        print("\n播放已停止")
    finally:
        stream.stop()
        stream.close()
        print("播放完成")


class P3PlayerApp:
    def __init__(self, root):
        self.root = root
        self.root.title("P3 文件简易播放器")
        self.root.geometry("500x400")

        self.playlist = []
        self.current_index = 0
        self.is_playing = False
        self.is_paused = False
        self.stop_event = threading.Event()
        self.pause_event = threading.Event()
        self.loop_playback = tk.BooleanVar(value=False)  # 循环播放复选框的状态

        # 创建界面组件
        self.create_widgets()

    def create_widgets(self):
        # 播放列表
        self.playlist_label = tk.Label(self.root, text="播放列表:")
        self.playlist_label.pack(pady=5)

        self.playlist_frame = tk.Frame(self.root)
        self.playlist_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.playlist_listbox = tk.Listbox(self.playlist_frame, selectmode=tk.SINGLE)
        self.playlist_listbox.pack(fill=tk.BOTH, expand=True)

        # 复选框和移除按钮
        self.checkbox_frame = tk.Frame(self.root)
        self.checkbox_frame.pack(pady=5)

        self.remove_button = tk.Button(self.checkbox_frame, text="移除文件", command=self.remove_files)
        self.remove_button.pack(side=tk.LEFT, padx=5)

        # 循环播放复选框
        self.loop_checkbox = tk.Checkbutton(self.checkbox_frame, text="循环播放", variable=self.loop_playback)
        self.loop_checkbox.pack(side=tk.LEFT, padx=5)

        # 控制按钮
        self.control_frame = tk.Frame(self.root)
        self.control_frame.pack(pady=10)

        self.add_button = tk.Button(self.control_frame, text="添加文件", command=self.add_file)
        self.add_button.grid(row=0, column=0, padx=5)

        self.play_button = tk.Button(self.control_frame, text="播放", command=self.play)
        self.play_button.grid(row=0, column=1, padx=5)

        self.pause_button = tk.Button(self.control_frame, text="暂停", command=self.pause)
        self.pause_button.grid(row=0, column=2, padx=5)

        self.stop_button = tk.Button(self.control_frame, text="停止", command=self.stop)
        self.stop_button.grid(row=0, column=3, padx=5)

        # 状态标签
        self.status_label = tk.Label(self.root, text="未在播放", fg="blue")
        self.status_label.pack(pady=10)

    def add_file(self):
        files = filedialog.askopenfilenames(filetypes=[("P3 文件", "*.p3")])
        if files:
            self.playlist.extend(files)
            self.update_playlist()

    def update_playlist(self):
        self.playlist_listbox.delete(0, tk.END)
        for file in self.playlist:
            self.playlist_listbox.insert(tk.END, os.path.basename(file))  # 仅显示文件名

    def update_status(self, status_text, color="blue"):
        """更新状态标签的内容"""
        self.status_label.config(text=status_text, fg=color)

    def play(self):
        if not self.playlist:
            messagebox.showwarning("警告", "播放列表为空！")
            return

        if self.is_paused:
            self.is_paused = False
            self.pause_event.clear()
            self.update_status(f"正在播放：{os.path.basename(self.playlist[self.current_index])}", "green")
            return

        if self.is_playing:
            return

        self.is_playing = True
        self.stop_event.clear()
        self.pause_event.clear()
        self.current_index = self.playlist_listbox.curselection()[0] if self.playlist_listbox.curselection() else 0
        self.play_thread = threading.Thread(target=self.play_audio, daemon=True)
        self.play_thread.start()
        self.update_status(f"正在播放：{os.path.basename(self.playlist[self.current_index])}", "green")

    def play_audio(self):
        while True:
            if self.stop_event.is_set():
                break

            if self.pause_event.is_set():
                time.sleep(0.1)
                continue

            # 检查当前索引是否有效
            if self.current_index >= len(self.playlist):
                if self.loop_playback.get():  # 如果勾选了循环播放
                    self.current_index = 0  # 回到第一首
                else:
                    break  # 否则停止播放

            file = self.playlist[self.current_index]
            self.playlist_listbox.selection_clear(0, tk.END)
            self.playlist_listbox.selection_set(self.current_index)
            self.playlist_listbox.activate(self.current_index)
            self.update_status(f"正在播放：{os.path.basename(self.playlist[self.current_index])}", "green")
            play_p3_file(file, self.stop_event, self.pause_event)

            if self.stop_event.is_set():
                break

            if not self.loop_playback.get():  # 如果没有勾选循环播放
                break  # 播放完当前文件后停止

            self.current_index += 1
            if self.current_index >= len(self.playlist):
                if self.loop_playback.get():  # 如果勾选了循环播放
                    self.current_index = 0  # 回到第一首

        self.is_playing = False
        self.is_paused = False
        self.update_status("播放已停止", "red")

    def pause(self):
        if self.is_playing:
            self.is_paused = not self.is_paused
            if self.is_paused:
                self.pause_event.set()
                self.update_status("播放已暂停", "orange")
            else:
                self.pause_event.clear()
                self.update_status(f"正在播放：{os.path.basename(self.playlist[self.current_index])}", "green")

    def stop(self):
        if self.is_playing or self.is_paused:
            self.is_playing = False
            self.is_paused = False
            self.stop_event.set()
            self.pause_event.clear()
            self.update_status("播放已停止", "red")

    def remove_files(self):
        selected_indices = self.playlist_listbox.curselection()
        if not selected_indices:
            messagebox.showwarning("警告", "请先选择要移除的文件！")
            return

        for index in reversed(selected_indices):
            self.playlist.pop(index)
        self.update_playlist()


if __name__ == "__main__":
    root = tk.Tk()
    app = P3PlayerApp(root)
    root.mainloop()
