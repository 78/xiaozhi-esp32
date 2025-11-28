## 目标

* 读取 `CHARGE_INPUT_GPIO` 电平：高=充电，低=不充电

* 设置 `CHARGE_STATUS_LED_GPIO`：充电时高电平，非充电时低电平

## 实现方案

* 初始化：

  * 将 `CHARGE_INPUT_GPIO` 配置为输入并启用下拉

  * 将 `CHARGE_STATUS_LED_GPIO` 配置为输出并设初始电平为低电平

* 状态维护：

  * 增加成员 `last_charge_level_`

* 任务循环：

  * 在设备状态监控任务中调用 `UpdateChargeStatus()`，当输入电平变化或每次轮询时同步指示灯电平

## 改动文件

* `main/boards/ai-martube-esp32s3/ai_martube_esp32s3.cc`

  * 添加 `InitializeChargeStatus()` 与 `UpdateChargeStatus()`

  * 在板卡初始化流程与任务循环中调用

## 验证

* 上电后根据插拔充电线验证指示灯高/低电平切换

* 串口日志打印当前充电状态变化

