# 配网热点不可见排查记录

日期：2026-11-16。目标板 Waveshare ESP32-S3 Touch LCD 3.5B，ESP-IDF v6.1，COM3。

## 现象与约束

设备打印 AP 启动、SSID `Xiaozhi-6EC1`、IP `192.168.4.1`，但电脑和用户反馈的客户端看不到热点。电脑只进行扫描，始终保持连接原 Wi-Fi，没有连接设备热点。保留 NVS、Wi-Fi 凭据与 API Key。

用 Windows Native Wi-Fi API 主动请求扫描，等待 scan_complete 通知，再读取可用网络和 BSSID 列表；不能将 WlanScan 返回成功解释为已完成扫描。设备 AP BSSID 为 `a4:cb:8f:d7:6e:c1`。

## 对照结果

| 实验 | 实机变化 | 扫描结果 |
| --- | --- | --- |
| 原状态 | 仅重新进入配网 | 多次主动扫描未发现 |
| 发射功率 | 8 / 80 / 8 / 80，每阶段 20 秒 | 12 次中仅 1 次发现，且发生在低功率阶段；升功率未解决 |
| Wi-Fi 模式 | APSTA / AP / APSTA / AP，功率保持 8 | 12 次中仅 1 次发现，不能归因于 APSTA 单一因素 |
| 只启停扫描 | 不改信道，保持功率 | 12 次中仅 1 次发现；单纯停止扫描未解决 |
| 停扫描并重新指定信道 | 信道 1 / 6 / 11 / 1，功率仍为 8 | 12 次中 9 次发现，BSSID 与频率可核对；切换信道后的首次扫描可能未发现 |
| 重新指定信道并恢复扫描 | 先保留原行为 20 秒，再停止扫描、重设信道 1、恢复扫描 | 之前 3 次均未发现；经过过渡后，后续 7 次连续发现，设备侧周期扫描完成日志仍存在 |

热点配置读取为 hidden=0、配置信道 1、Beacon 间隔 100 ms。功率 8 对应 SDK 接口的 2 dBm，虽然低，但热点可见时 RSSI 约 -44 至 -52 dBm，不支持将低功率直接认定为原因。

当前证据把问题定位在 STA 转入 APSTA 后的信道恢复路径：仅停止扫描无效，而停止扫描后重新指定广播信道可恢复发现。底层 Wi-Fi 驱动内部的具体原因尚未证实，不应描述为已确认的某个 SDK 内部缺陷。

## 最小修复

位置：`main/boards/common/wifi_board.cc` 的 `StartWifiConfigMode()`。仅在 ESP32-S3 热点配网路径，AP 启动后读取其配置，停止初始扫描，重新应用配置中的信道，再启动扫描。记录各操作结果，失败时输出警告。

保留 APSTA、原功率和原有扫描/连接接口；不写 NVS、不改引脚、不改 vendor 文件。所有自动切换功率/模式/信道的临时实验任务已删除。

## 构建、烧录与最终复测

最小修复已构建并烧录，esptool 报告 Hash of data verified / Done。固件 SHA-256：`78dc7c8c436f5efd0741364c530c7671f4fcf40c8de9d18522a608d0661e2772`。

烧录后的重启曾出现 TCA9554 I²C 初始化超时，无效 IO 扩展器句柄随后触发 ESP_ERROR_CHECK，设备在 Wi-Fi 初始化前反复重启。用户彻底断电再上电后恢复启动，接着的软件复位也正常。此问题发生在本次信道修复代码执行之前，但其根因尚未定位。

同一最终固件完成两轮实测：

| 轮次 | 进入配网前 | 主动扫描结果 |
| --- | --- | --- |
| 用户断电上电后 | Auto / idle，已连接原网络，API Key 仍存在 | 12 次发现 9 次；第 2～5 次及第 9～12 次分别连续发现 |
| 软件复位后 | Auto / idle，重新连接原网络，API Key 仍存在 | 8 次发现 4 次，第 2、3、4、7 次命中 |

两轮均打印 `Provisioning AP channel=1 restore=ESP_OK scan=ESP_OK`。核对实际 BSSID `a4:cb:8f:d7:6e:c1`，频率 2412 MHz（信道 1），命中时 RSSI 为 -44～-54 dBm。电脑保持连接 `ZTE-NyDCAy-wifi5`，没有连接设备热点。测试结束设备留在配网模式，串口已释放。

验收结论：已恢复电脑重复发现热点的能力，但两轮合计 13/20 次命中，仍有漏扫，不能宣称彻底修复或长期稳定。尚不能区分剩余漏扫来自设备广播、APSTA 周期扫描离开主信道，还是 Windows 客户端扫描行为；需要独立客户端或无线抓包进一步分离。信道重设是当前实测有效的缓解措施，底层根因仍未完全闭环。

主机测试再次运行 147 项，28 跳过、6 个错误，与此前环境问题相同（5 个 Windows 目录锁定/清理错误、1 个缺少主机 C++ 编译器）。目标板构建、烧录、触及文件的 clang-format 与补丁空白检查通过；未补做其他芯片构建。

## 证据文件

本机临时目录保留 `xiaozhi-ap-<power|mode|channel|scan|retune>-only-serial.log` 与对应 `*-scans.jsonl`。构建/烧录日志为 `xiaozhi-ap-fix-build.log`、`xiaozhi-ap-fix-flash.log`。录制/扫描脚本和临时实验源码在临时目录，不进入固件产品路径。

最终固件两轮证据为 `xiaozhi-ap-fix-final-serial.log`、`xiaozhi-ap-fix-final-scans.jsonl` 和 `xiaozhi-ap-fix-reboot-serial.log`、`xiaozhi-ap-fix-reboot-scans.jsonl`；主机测试日志为 `xiaozhi-ap-host-tests.log`。

热点实际配网、浏览器页面和提交凭据不在本轮扫描验收范围内。
