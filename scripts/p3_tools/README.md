# P3音频格式转换与播放工具

这个目录包含两个用于处理P3格式音频文件的Python脚本：

## 1. 音频转换工具 (convert_audio_to_p3.py)

将普通音频文件转换为P3格式（4字节header + Opus数据包的流式结构）。

### 使用方法

```bash
python convert_audio_to_p3.py <输入音频文件> <输出P3文件>
```

例如：
```bash
python convert_audio_to_p3.py input.mp3 output.p3
```

## 2. P3音频播放工具 (play_p3.py)

播放P3格式的音频文件。

### 特性

- 解码并播放P3格式的音频文件
- 在播放结束或用户中断时应用淡出效果，避免破音
- 支持通过命令行参数指定要播放的文件

### 使用方法

```bash
python play_p3.py <P3文件路径>
```

例如：
```bash
python play_p3.py output.p3
```

## 依赖安装

在使用这些脚本前，请确保安装了所需的Python库：

```bash
pip install librosa opuslib numpy tqdm sounddevice
```

或者使用提供的requirements.txt文件：

```bash
pip install -r requirements.txt
```

## P3格式说明

P3格式是一种简单的流式音频格式，结构如下：
- 每个音频帧由一个4字节的头部和一个Opus编码的数据包组成
- 头部格式：[1字节类型, 1字节保留, 2字节长度]
- 采样率固定为16000Hz，单声道
- 每帧时长为60ms 