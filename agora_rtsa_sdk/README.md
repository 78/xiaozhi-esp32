# Agora RTSA-Lite SDK 介绍

*简体中文| [English](README.en.md)*

声网实时码流加速（Real-time Streaming Acceleration, RTSA）SDK 版，依托声网自建的底层实时传输网络 Agora SD-RTN™ (Software Defined Real-time Network)，为所有支持网络功能的 Linux/RTOS 设备提供音视频码流在互联网实时传输的能力。RTSA 充分利用了声网全球全网节点和智能动态路由算法，与此同时支持了前向纠错、智能重传、带宽预测、码流平滑等多种组合抗弱网的策略，可以在设备所处的各种不确定网络环境下，仍然交付高连通、高实时和高稳定的最佳码流传输体验。此外，该 SDK 具有极小的包体积和内存占用，适合运行在典型的 IoT 设备上。

# 五分钟快速入门

我们提供了一个简单的示例项目(hello_rtsa)，演示如何通过RTSA进行音视频的推流，并通过网页端实时观看效果。

## 1. 创建 Agora 账号并获取 App ID

在编译和运行示例项目前，你首先需要通过以下步骤获取 Agora App ID:
1. 创建一个有效的 [Agora 账号](https://console.agora.io/)。
2. 登录 [Agora 控制台](https://console.agora.io/)，点击左侧导航栏项目管理按钮进入项目管理页面。
3. 在项目管理页面，点击创建按钮。在弹出的对话框内输入项目名称，选择鉴权机制为 App ID。点击提交，新建的项目就会显示在项目管理页中。Agora 会给每个项目自动分配一个 App ID 作为项目唯一标识。复制并保存此项目的 **App ID** ，稍后运行示例项目时会用到 App ID。

## 2. 编译 hello_rtsa

在Linux x86环境下（比如Ubuntu 18.04），通过以下命令编译 hello_rtsa。
```
$ cd example
$ ./build-x86_64.sh
```
编译完成后，会在当前目录里创建out/x86_64/目录，并在该目录下生成 hello_rtsa 可执行文件。

## 3. 运行 hello_rtsa

你可通过以下命令行，把 YOUR_APP_ID 替换称你在第一步中获取到的 App ID，加入名为 hello_demo 的频道，并且用默认参数发送 H.264 视频和 PCM 音频。默认音视频源路径固定为 `../test_data/send_video.h264` 和 `../test_data/send_audio_16k_1ch.pcm`，因此必须在 `out/` 目录下执行程序。

```
$ cd out
$ ./x86_64/hello_rtsa -i YOUR_APPID -c hello_demo
```

请注意：参数 `YOUR_APPID` 就是你刚才创建的 App ID。

请注意：如果在创建项目时，鉴权机制未设置为 App ID，而是设置为 Token，运行时将出现 `Error 105 is captured.` 错误提示，请重新创建项目选择 App ID 鉴权，或者参考官网文档生成临时 Token 并使用 `-t YOUR_TOKEN`设置 Token 参数，再次尝试运行。

当终端上出现 `Join the channel "hello_demo" successfully` 的打印时，说明已经成功加入频道并开始推流了。

## 4. 观看推流效果
你可以打开[Webdemo链接](https://webdemo.agora.io/agora-websdk-api-example-4.x/basicVideoCall/index.html)，并在 `App ID` 栏输入 `YOUR_APPID` ，在 `Channels`栏输入 `hello_demo`，点击 JOIN，随即就能看到和听到实时音视频流了！

# 移植

为了能让示例项目（hello_rtsa）运行在嵌入式设备端（通常是ARM Linux系统），请参考 [移植指南](./docs/PORTING.md)

# 集成
在项目的集成过程中，你可能需要了解以下话题:
- [基础音视频互通](https://docs.agora.io/cn/RTSA/transmit_streams_linux?platform=Linux)
- [更安全的Token机制](https://docs.agora.io/cn/Agora%20Platform/token)
- [License使用指南](https://docs-preprod.agora.io/cn/Agora%20Platform/agora_console_license_overview?platform=Android)

# 关于 License
为了让你可以流畅体验我们的示例项目，并快速开始集成和测试你自己的项目，我们的 SDK 提供了一定时长的免费试用期。免费期到期之后，则无法继续使用 RTSA SDK，示例项目（hello_rtsa）也将无法运行！

所以，在免费期到期或正式上线之前，请务必联系声网销售(iot@agora.io)，购买商用的License，并且在项目中集成 License 的激活机制。详细流程请参考 [License集成指南]()

# 联系我们

- 如果发现了示例代码的 bug，欢迎[提交工单](https://agora-ticket.agora.io/)
