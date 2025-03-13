# 将音频文件转换为协议 v3 流
import librosa  # 用于加载和处理音频文件
import opuslib  # 用于编码音频为 Opus 格式
import struct   # 用于打包二进制数据
import sys      # 用于处理命令行参数
import tqdm     # 用于显示进度条
import numpy as np  # 用于处理音频数据

def encode_audio_to_opus(input_file, output_file):
    """
    将输入的音频文件编码为 Opus 格式，并保存为协议 v3 流格式。
    
    参数:
    - input_file: 输入音频文件的路径
    - output_file: 输出文件的路径
    """
    # 使用 librosa 加载音频文件
    # sr=None 表示保留原始采样率，mono=False 表示保留多声道（如果存在）
    audio, sample_rate = librosa.load(input_file, sr=None, mono=False, dtype=np.float32)
    
    # 如果需要，将采样率转换为 16000Hz
    target_sample_rate = 16000
    if sample_rate != target_sample_rate:
        audio = librosa.resample(audio, orig_sr=sample_rate, target_sr=target_sample_rate)
        sample_rate = target_sample_rate
    
    # 如果是立体声，只取左声道
    if audio.ndim == 2:
        audio = audio[0]
    
    # 将音频数据从浮点数转换为 int16 格式
    audio = (audio * 32767).astype(np.int16)
    
    # 初始化 Opus 编码器
    # 参数: 采样率, 声道数 (1 表示单声道), 应用类型 (VOIP 表示语音优化)
    encoder = opuslib.Encoder(sample_rate, 1, opuslib.APPLICATION_VOIP)

    # 将音频数据编码为 Opus 数据包，并保存到文件
    with open(output_file, 'wb') as f:
        duration = 60  # 每帧的时长为 60ms
        frame_size = int(sample_rate * duration / 1000)  # 计算每帧的样本数
        # 使用 tqdm 显示进度条
        for i in tqdm.tqdm(range(0, len(audio) - frame_size, frame_size)):
            # 获取当前帧的音频数据
            frame = audio[i:i + frame_size]
            # 使用 Opus 编码器编码当前帧
            opus_data = encoder.encode(frame.tobytes(), frame_size=frame_size)
            # 按照协议 v3 格式打包数据: [1u type, 1u reserved, 2u len, data]
            packet = struct.pack('>BBH', 0, 0, len(opus_data)) + opus_data
            # 将打包后的数据写入文件
            f.write(packet)

# 示例用法
if len(sys.argv) != 3:
    print('Usage: python convert.py <input_file> <output_file>')
    sys.exit(1)

# 从命令行参数获取输入文件和输出文件路径
input_file = sys.argv[1]
output_file = sys.argv[2]
# 调用函数进行音频编码
encode_audio_to_opus(input_file, output_file)