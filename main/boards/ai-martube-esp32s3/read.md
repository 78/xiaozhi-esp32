# 调试记录
    1.时间：2025.11.07
      问题描述：无法启用SDK配置编辑器
      修改文件：main\idf_component.yml
      修改内容：espressif2022/image_player: ==1.1.0~1更改为espressif2022/image_player: ==1.1.0
      问题原因：- 由于依赖的更新导致编译不通过

# 功能调整
    1.时间：2025.11.14
      问题描述：输出音量范围调整为30-80
      修改文件：main\audio\audio_codec.cc
      修改内容：将输出音量范围从0-100调整为30-80
      问题原因：- 原始范围0-100当音量超过80时，会出现震动位移有时功率超了导致电池电压拉低，调整为30-80更安全

# 本地语音生成与调用
    1.将原始MP3音频转为OGG格式
      在根目录打开终端，执行以下命令：
      cd e:\code\project\martube_ai_speaker\xiaozhi-esp32 ; $ff = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Gyan.FFmpeg.Essentials_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-8.0-essentials_build\bin\ffmpeg.exe"; & $ff -y -i "main/assets/common/{需要转换的音频}.mp3" -acodec libopus -b:a 16k -ac 1 -ar 16000 "main/assets/common/{自定义转化后的音频名称}.ogg"
    2.在终端执行“idf.py clean", 重新编译，在“main\assets\lang_config.h”中将生成音频宏定义

    3.调用音频
      在需要调用自定义音频的地方，使用OGG_BATTERYREMIND即可。
      例如：
      app.PlaySound(Lang::Sounds::OGG_BATTERYOFF);



