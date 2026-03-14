# ogg_covertor 小智AI OGG 音效批量转换器

本脚本为OGG批量转换工具，支持将输入的音频文件转换为小智可使用的OGG格式

基于Python第三方库 `ffmpeg-python` 实现，**需要** `ffmpeg` 环境  

可前往[此处](https://ffmpeg.org/download.html)下载对应你自己系统的ffmpeg发行版，并添加到环境变量或者放在脚本所在目录

支持OGG和音频之间的互转，响度调节等功能

# 创建并激活虚拟环境

```bash
# 创建虚拟环境
python -m venv venv
# 激活虚拟环境
source venv/bin/activate # Mac/Linux
venv\Scripts\activate # Windows
```
# 下载FFmpeg
前往[此处](https://ffmpeg.org/download.html)下载ffmpeg  

根据你当前的系统下载对应的版本，并将`ffmpeg`的可执行文件放置在脚本所在目录或者添加可执行文件所在目录到环境变量

# 安装依赖
请在虚拟环境中执行

```bash
pip install ffmpeg-python
```

# 运行脚本
```bash
python ogg_covertor.py
```

