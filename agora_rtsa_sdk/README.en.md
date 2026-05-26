# Agora RTSA-Lite SDK Introduction

*[简体中文](README.md) | English*

The Agora Real-time Streaming Acceleration (RTSA) SDK is built on Agora's self-developed
real-time transport network, Agora SD-RTN™ (Software Defined Real-time Network), and provides
real-time audio and video streaming capabilities over the Internet for Linux and RTOS devices with
network connectivity. RTSA fully leverages Agora's global edge network and intelligent dynamic
routing algorithms. At the same time, it supports multiple weak-network resilience strategies such
as forward error correction, intelligent retransmission, bandwidth estimation, and stream pacing,
delivering high connectivity, low latency, and stable media transport even under uncertain network
conditions. In addition, the SDK has a very small package size and memory footprint, making it
well suited for typical IoT devices.

# Five-Minute Quick Start

We provide a simple sample project, `hello_rtsa`, to demonstrate how to push audio and video
streams through RTSA and watch the result in real time from a web client.

## 1. Create an Agora Account and Get an App ID

Before building and running the sample, you need to get an Agora App ID by following these steps:

1. Create a valid [Agora account](https://console.agora.io/).
2. Sign in to the [Agora Console](https://console.agora.io/) and enter the Project Management page
   from the left navigation panel.
3. On the Project Management page, click Create. In the dialog box, enter a project name and
   choose **App ID** as the authentication mechanism. Click Submit. The new project will then
   appear in the project list. Agora automatically assigns a unique App ID to each project. Copy
   and save this **App ID**. You will need it when running the sample.

## 2. Build hello_rtsa

On an x86 Linux environment such as Ubuntu 18.04, build `hello_rtsa` with the following commands:

```bash
$ cd example
$ ./build.sh -a x86_64
```

After the build completes, an `out/x86_64/` directory is created in the current directory, and the
`hello_rtsa` executable is generated there.

## 3. Run hello_rtsa

Run the following commands, replacing `YOUR_APPID` with the App ID you obtained in step 1. The
sample joins a channel named `hello_demo` and sends H.264 video and PCM audio using the default
parameters. The default media source paths are fixed as `../test_data/send_video.h264` and
`../test_data/send_audio_16k_1ch.pcm`, so you must run the program from the `out/` directory.

```bash
$ cd out
$ ./x86_64/hello_rtsa -i YOUR_APPID -c hello_demo
```

Note: `YOUR_APPID` is the App ID of the project you just created.

Note: If the authentication mechanism of your project is not set to App ID but to Token, the sample
will report `Error 105 is captured.` at runtime. In that case, either create a new project with
App ID authentication, or generate a temporary token according to Agora documentation and run the
sample again with `-t YOUR_TOKEN`.

When `Join the channel "hello_demo" successfully` appears in the terminal, it means the sample has
joined the channel successfully and started streaming.

## 4. Watch the Streaming Result

Open the [Web demo](https://webdemo.agora.io/agora-websdk-api-example-4.x/basicVideoCall/index.html),
enter `YOUR_APPID` in the `App ID` field, enter `hello_demo` in the `Channels` field, and click
JOIN. You will then be able to see and hear the live audio and video stream.

# Porting

To run the sample project (`hello_rtsa`) on embedded devices, typically ARM Linux systems, refer to
the [Porting Guide](./docs/PORTING.md).

# Integration

During integration, you may also want to read about the following topics:

- [Basic audio and video streaming](https://docs.agora.io/cn/RTSA/transmit_streams_linux?platform=Linux)
- [A more secure token mechanism](https://docs.agora.io/cn/Agora%20Platform/token)
- [License usage guide](https://docs-preprod.agora.io/cn/Agora%20Platform/agora_console_license_overview?platform=Android)

# About License

To let you experience our sample project smoothly and quickly begin integrating and testing your own
project, the SDK provides a limited free trial period. After the free period expires, you can no
longer continue to use the RTSA SDK, and the sample project (`hello_rtsa`) will also stop working.

Therefore, before the trial period expires or before your product goes live, please contact Agora
Sales at `iot@agora.io` to purchase a commercial license and integrate the license activation
mechanism into your project. For the detailed process, refer to the License Integration Guide.

# Contact Us

- If you find a bug in the sample code, please [submit a ticket](https://agora-ticket.agora.io/).
