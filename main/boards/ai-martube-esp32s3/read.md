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



I (1139164) Application: >> 现���天气怎么样？
W (1139174) Display: Role:user
W (1139174) Display:      现在天气怎���样？
I (1139894) Application: << % get_weather...
W (1139894) Display: Role:assistant
W (1139894) Display:      % get_weather...
I (1140084) ai_martube_esp32s3: Key state changed: 1 -> 0
I (1140084) ai_martube_esp32s3: Key pressed at ld
W (1140124) Display: SetEmotion: neutral
I (1140374) ai_martube_esp32s3: WiFi RSSI: -48 dBm
I (1140814) Application: << 深���现在阴着天呢，19℃，���北风2级，云层厚厚的��盖了层毯子。
W (1140814) Display: Role:assistant
W (1140814) Display:      深圳现在阴着天呢，19℃，东北风2级，云层厚厚的像盖了���毯子。
I (1141814) ai_martube_esp32s3: Battery: 3.77V (43%)
I (1142374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1143094) ai_martube_esp32s3: Long press detected (instant)
I (1143094) Application: Abort speaking
I (1143094) Application: STATE: idle
W (1143094) Display: SetStatus: 待命
W (1143094) Display: SetEmotion: neutral
I (1143094) ai_martube_esp32s3: Power amplifier enabled on GPIO 38
I (1143104) AudioService: OpusHead: version=1, channels=1, sample_rate=16000
I (1143114) AudioService: Resampling audio from 16000 to 24000
I (1143114) OpusResampler: Resampler configured with input sample rate 16000 and output sample rate 24000
I (1144374) ai_martube_esp32s3: WiFi RSSI: -48 dBm
I (1145964) ai_martube_esp32s3: Power amplifier disabled on GPIO 38
I (1145964) ai_martube_esp32s3: Power amplifier disabled on GPIO 38
I (1145964) UartComm: UART TX write=4 len=4
I (1145964) ai_martube_esp32s3: Long press: switch to Bluetooth mode, sent A5 00 02 07
I (1145984) ai_martube_esp32s3: Device state changed to idle, switching to Bluetooth audio mode
I (1145984) ai_martube_esp32s3: Power amplifier disabled on GPIO 38
I (1145984) ai_martube_esp32s3: Key state changed: 0 -> 1
I (1145994) ai_martube_esp32s3: Key released, duration: ld us
I (1146374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1146824) ai_martube_esp32s3: Battery: 3.78V (44%)
I (1148374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1150374) ai_martube_esp32s3: WiFi RSSI: -51 dBm
I (1151824) ai_martube_esp32s3: Battery: 3.77V (43%)
I (1152374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1152474) SystemInfo: free sram: 125763 minimal sram: 121731
I (1154374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1156374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1156824) ai_martube_esp32s3: Battery: 3.78V (44%)
I (1158374) ai_martube_esp32s3: WiFi RSSI: -52 dBm
I (1160374) ai_martube_esp32s3: WiFi RSSI: -51 dBm
I (1160624) I2S_IF: Pending out channel for in channel running
I (1160624) AudioCodec: Set output enable to false
I (1161824) ai_martube_esp32s3: Battery: 3.77V (43%)
I (1162374) ai_martube_esp32s3: WiFi RSSI: -51 dBm
I (1162474) SystemInfo: free sram: 125763 minimal sram: 121731
I (1164374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1166374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1166834) ai_martube_esp32s3: Battery: 3.78V (44%)
I (1168374) ai_martube_esp32s3: WiFi RSSI: -48 dBm
I (1170374) ai_martube_esp32s3: WiFi RSSI: -48 dBm
I (1171834) ai_martube_esp32s3: Battery: 3.77V (43%)
I (1172374) ai_martube_esp32s3: WiFi RSSI: -48 dBm
I (1172474) SystemInfo: free sram: 126279 minimal sram: 121731
I (1174374) ai_martube_esp32s3: WiFi RSSI: -47 dBm
I (1176374) ai_martube_esp32s3: WiFi RSSI: -48 dBm
I (1176844) ai_martube_esp32s3: Battery: 3.78V (44%)
I (1178374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1180374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1181854) ai_martube_esp32s3: Battery: 3.78V (44%)
I (1182374) ai_martube_esp32s3: WiFi RSSI: -48 dBm
I (1182474) SystemInfo: free sram: 125763 minimal sram: 121731
I (1184374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1186374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1186864) ai_martube_esp32s3: Battery: 3.77V (43%)
I (1188374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1190374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1191864) ai_martube_esp32s3: Battery: 3.78V (43%)
I (1192374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1192474) SystemInfo: free sram: 126279 minimal sram: 121731
I (1194374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1196374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1196864) ai_martube_esp32s3: Battery: 3.77V (42%)
I (1198374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1200374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1201874) ai_martube_esp32s3: Battery: 3.78V (44%)
I (1202374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1202474) SystemInfo: free sram: 125763 minimal sram: 121731
I (1204374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1204664) Application: STATE: speaking
W (1204664) Display: SetStatus: 说话中...
I (1204674) ai_martube_esp32s3: Power amplifier enabled on GPIO 38
I (1204674) ai_martube_esp32s3: Device state changed to speaking, ensuring ESP32S3 output
I (1205064) Application: << 晚���啦，小星星～
W (1205064) Display: Role:assistant
W (1205064) Display:      晚安啦��小星星～
I (1206374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1206874) ai_martube_esp32s3: Battery: 3.77V (43%)
I (1207274) Application: << 愿���的梦里有温柔的风和闪烁的光。
W (1207274) Display: Role:assistant
W (1207274) Display:      愿你的梦里有���柔的风和闪烁的光。
I (1208374) ai_martube_esp32s3: WiFi RSSI: -49 dBm
I (1210374) ai_martube_esp32s3: WiFi RSSI: -50 dBm
I (1210874) Application: STATE: listening
W (1210874) Display: SetStatus: 聆听中...
W (1210874) Display: SetEmotion: neutral
I (1210884) ai_martube_esp32s3: Device state changed to listening, ensuring ESP32S3 input
I (1210884) ai_martube_esp32s3: Power amplifier disabled on GPIO 38
I (1211084) MQTT: Received goodbye message, session_id: cadd8494
I (1211094) wifi:Set ps type: 1, coexist: 0

W (1211094) Display: Role:system
W (1211094) Display:      
I (1211094) Application: STATE: idle
W (1211094) Display: SetStatus: 待命
W (1211094) Display: SetEmotion: neutral
I (1211104) ai_martube_esp32s3: Device state changed to idle, switching to Bluetooth audio mode
I (1211114) ai_martube_esp32s3: Power amplifier disabled on GPIO 38
