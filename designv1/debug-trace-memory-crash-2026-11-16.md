# 调试音频录制内存分配失败导致重启

日期：2026-11-16。Waveshare ESP32-S3 Touch LCD 3.5B，ESP-IDF v6.1，COM3。

## 日志定位

用户日志约 63.978 秒进入 speaking 后触发 abort，回溯为：`AliyunProtocol::HandleEvent(response.audio.delta)` → `DebugTrace::Pcm(false, ...)` → `DebugTrace::Queue()` → `vector::reserve()` → `operator new` → `__cxa_allocate_exception` → `abort`。

直接原因是诊断音频帧申请内存失败。日志未给出失败时各内存区域的可用量、最大连续块和申请大小，尚不能区分内部 RAM、PSRAM、碎片或泄漏。较早的代理连接失败随后已经恢复，不是此处 abort 的直接路径；时间未同步的 Display 警告也不在崩溃栈中。

旧代码先 reserve / 复制完整帧，再检查队列字节上限，因此容量保护无法防止该次临时分配失败；std::deque 插入节点也可能再进行一次会抛异常的分配。

## 修复

修改 `main/realtime/debug_trace.cc` 和 `.h`：

- 分配、复制负载之前检查队列字节数和帧数。
- 每帧通过一次可检查失败的 heap_caps_malloc 分配链表节点和帧数据，移除帧存储的 vector / deque 分配。
- PSRAM 构建将帧明确分配到 PSRAM，不退回内部 RAM；队列上限 512 KiB。无 PSRAM 构建上限 32 KiB。均最多 256 帧，并计入节点字节数。
- 分配失败时丢弃本次诊断数据，累计 allocation_drops，最多每 5 秒警告一次；后续可入队时记录 trace_gap。溢出同样记录 gap，发送或淘汰后释放内存。
- hello 改用固定 12 字节栈上帧头及已有 JSON 字符串发送，线上协议保持一致。

没有修改云端播放、ES8311 采集、Auto 半双工或配网信道逻辑。

## 构建与实机结果

目标板构建及 COM3 烧录成功，esptool 校验 Hash of data verified / Done。固件 SHA-256：`16c5ef3c81a829b310ea6ed21e0f8d7d8729c34dd4039eeb1c08996e01ac1f0e`。应用分区剩余 37%；触及文件的 clang-format 和补丁空白检查通过。

保持 Auto、诊断等级 2、原始 PCM 开启；未写设备配置或导出 API Key。

1. 只启动代理，暂不监听设备诊断地址 7319。电脑扬声器播放“请用中文从一数到三十，直接开始数”。云端识别和回复完整，设备 listening → speaking → listening，约 62 秒串口观察未见 abort / panic，内部空闲 SRAM 约 59～61 KiB。
2. 恢复诊断收集器后自动连入。会话 `02a4b354192ce55ca6838425` 收到 256 条 trace_gap（allocation_drops=0，实际触发的是队列容量分支），随后恢复原始输入和上行记录。积压期间的录音已丢失，不能宣称完整。
3. 结束旧会话，重新测试“请用中文从一数到十，直接开始数”。识别、回复与播放完成正常。新诊断会话 `fefcef917cbbca3cf82a646b` 的 gap=0，原始输入、上行、下行三轨分别记录 3,058,080 / 1,833,600 / 264,960 字节。
4. 两轮设备 boot_id 均为 `a78d6feecc4dbf77`，中途未重启。结束后 stop 到 Auto / idle，API Key 仍存在，串口释放。只关闭本轮启动的两个测试服务，将端口交还用户的 VS Code Go Debug。

主机测试 147 项，28 跳过、6 个错误，与此前基线一致：5 个 Windows 目录锁定/清理错误、1 个缺少主机 C++ 编译器。Go 源码未改，测试服务重新构建成功，实际收集器接受并解析了新固件帧。

## 证据与限制

临时目录保留 `xiaozhi-trace-oom-build.log`、`xiaozhi-trace-oom-flash.log`、`xiaozhi-trace-oom-serial.log`、`xiaozhi-trace-oom-connected-serial.log`、`xiaozhi-trace-oom-host-tests.log`。录音保存在 `%LOCALAPPDATA%\xiaozhi-trace-oom-validation` 与 `%LOCALAPPDATA%\xiaozhi-trace-oom-collector`，不入库。

实测覆盖队列积压、溢出、重连恢复和正常对话。未主动耗尽整块 PSRAM，分配函数返回空指针分支未做故障注入；未完成长时间压力、无 PSRAM 板型及其他芯片验证，不能解释为所有内存耗尽场景均已解决。
