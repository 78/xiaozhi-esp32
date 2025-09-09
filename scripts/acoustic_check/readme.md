# 声波测试
该gui用于测试接受小智设备通过`udp`回传的`pcm`转时域/频域, 可以保存窗口长度的声音, 用于判断噪音频率分布和测试声波传输ascii的准确度,

固件测试需要打开`USE_AUDIO_DEBUGGER`, 并设置好`AUDIO_DEBUG_UDP_SERVER`是本机地址.
声波`demod`可以通过`sonic_wifi_config.html`或者上传至`PinMe`的[小智声波配网](https://iqf7jnhi.pinit.eth.limo)来输出声波测试

# 声波解码测试记录

> `✓`代表在I2S DIN接收原始PCM信号时就能成功解码, `△`代表需要降噪或额外操作可稳定解码, `X`代表降噪后效果也不好(可能能解部分但非常不稳定)。
> 个别ADC需要I2C配置阶段做更精细的降噪调整, 由于设备不通用暂只按照boards内提供的config测试

| 设备 | ADC | MIC | 效果 | 备注 |
| ---- | ---- | --- | --- | ---- |
| bread-compact | INMP441 | 集成MEMEMIC | ✓ |
| atk-dnesp32s3-box | ES8311 | | ✓ |
| magiclick-2p5 | ES8311 | | ✓ |
| lichuang-dev  | ES7210 | | △ | 测试时需要关掉INPUT_REFERENCE
| kevin-box-2 | ES7210 | | △ | 测试时需要关掉INPUT_REFERENCE
| m5stack-core-s3 | ES7210 | | △ | 测试时需要关掉INPUT_REFERENCE
| xmini-c3 | ES8311 | | △ | 需降噪
| atoms3r-echo-base | ES8311 | | △ | 需降噪
| atk-dnesp32s3-box0 | ES8311 | | X | 能接收且解码, 但是丢包率很高
| movecall-moji-esp32s3 | ES8311 | | X | 能接收且解码, 但是丢包率很高