# LCDWiki ES3C28P 待机天气仪表盘设计

日期：2026-09-22
分支：`feat/lcdwiki-es3c28p`
适用板子：`lcdwiki-es3c28p`（2.8 寸 240×320 ILI9341，FT6336 触摸）

## 1. 目标与范围

设备不聊天（`kDeviceStateIdle`）时，全屏显示待机仪表盘，包含：

- 顶部：WiFi 网络状态、小字当前时间
- 城市名、AQI 等级徽章（优/良/轻度污染…）、天气图标
- 空气指数数值、天气文字标签（阴/晴/雨…）
- 大字体当前时间（时:分:秒，秒为红色）、日期、星期
- 温度（带进度条）、湿度（带进度条）
- 右下角小机器人待机动画（GIF）

数据全部来自在线服务：IP 自动定位 + 和风天气 API。

### 不在本次范围（YAGNI）

- 本地温湿度传感器
- 多日/逐小时预报
- 新闻滚动条
- 语音查询天气（仅保留语音设置 API Key 的工具）
- 其他板子适配（代码可复用，但不主动接入）

## 2. 现状依据（代码库事实）

- 待机状态 `kDeviceStateIdle`：`main/device_state.h`；Application 的 idle 处理在 `main/application.cc` 的 `HandleStateChangedEvent`（约 :1009-1021）
- 1 秒心跳事件 `MAIN_EVENT_CLOCK_TICK`：`main/application.h:31`，处理在 `application.cc:274-285`
- 系统时钟在设备激活时由服务端下发（`main/ota.cc:193-216`，含时区偏移），无 SNTP；时间同步前 `tm_year < 2025-1900`
- WiFi 状态图标接口：`WifiBoard::GetNetworkStateIcon()`（`main/boards/common/wifi_board.cc:246-263`），按 RSSI 分档；WiFi 连接/断开事件在 `application.cc:110-140`
- 彩屏 UI：`main/display/lcd_display.cc`，lcdwiki 板使用微信气泡风格
- GIF 播放：`main/display/lvgl_display/gif/`（LvglGif）；现成 otto 机器人 GIF 在 `managed_components/txp666__otto-emoji-gif-component/gifs/`
- 现有正文字体最大 30px，无大时钟字体
- 仓库当前无任何天气数据链路，需要新建

## 3. 整体行为与数据流

### 切换时机

- 进入 idle：仪表盘全屏淡入（300ms），聊天气泡界面隐藏
- 离开 idle（listening/speaking/connecting 等任意状态）：仪表盘淡出，回到聊天界面
- 由 Application 的状态变化事件统一驱动，仪表盘不自行判断设备状态

### 三条数据线

1. **时间**：复用系统时钟 + 1 秒心跳。时钟每秒刷新（秒数变化）；日期、星期每分钟检查一次。
2. **WiFi 状态**：复用 `GetNetworkStateIcon()`，断网时图标置灰，天气区可继续显示上次数据并标注过期。
3. **天气**：
   - WiFi 连接后：`GeoIpLocator` 请求 `http://ip-api.com/json/?lang=zh-CN&fields=status,city,lat,lon`（5 秒超时），得到城市名与经纬度，结果缓存 24 小时
   - `QWeatherClient` 请求和风天气（HTTPS）：
     - `/v7/weather/now`：温度、湿度、天气文字、图标代码
     - `/v7/air/now`：AQI 数值、类别（优/良/…）
   - 正常刷新周期 30 分钟；请求失败 5 分钟后重试
   - WiFi 断开期间暂停天气循环，重连后立即刷新一次
- 所有网络请求运行在独立任务中，不阻塞主线程与音频

### API Key 管理

- Key 不写死在代码中，保存在 NVS（命名空间 `weather`，键 `qweather_key`）
- 通过新增 MCP 工具 `self.system.set_weather_api_key` 设置（语音：“设置天气密钥：xxx”），工具描述须说明密钥保存在设备本机
- 未设置 Key 时不发起和风请求

## 4. UI 布局（240×320 竖屏）

全屏 `dashboard_container`（默认隐藏），四个区域：

### ① 顶部状态栏（高 28px）

- 左：WiFi 信号图标（material symbols 20px 图标字体）
- 右：小字 HH:MM（20px 文本字体）

### ② 天气区（约 120px）

- 第一行：城市名（橙色，20px）｜AQI 徽章（圆角矩形，白字，颜色按分档）｜天气图标（彩色 noto emoji PNG）
- 第二行："空气指数 N"（20px）+ 右侧天气文字灰底白字小标签
- AQI 分档（国标）：0-50 优（绿）/ 51-100 良（黄）/ 101-150 轻度（橙）/ 151-200 中度（红）/ >200 重度（紫）

### ③ 时钟区（约 110px）

- 大时钟 HH:MM:SS：新增 72px 数字字体（Noto Sans 生成，仅含数字与冒号，1bpp 或 4bpp）；小时黑色、分钟橙色、秒红色（参照照片样式）
- 下方一行：日期 MM-DD（左）、星期"周X"（右，20px 现有文本字体，中文周一至周日）

### ④ 底部环境区（约 60px）

- 温度计图标（material symbols）+ 蓝色 LVGL bar（温度映射范围 -10°C ~ 40°C，两端钳制）+ "17°C"
- 水滴图标 + 绿色 LVGL bar（湿度 0-100%）+ "48%"
- 右下角：56×56 otto neutral GIF 动画

### 动效

- 仅仪表盘整体淡入/淡出（`lv_obj_fade_in/fade_out`，300ms）
- 不增加其他持续动画，避免 SPI 带宽与 CPU 压力（GIF 播放除外）

### 新增资源

- 72px 数字字体一个（构建脚本生成或直接提供转换后的 LVGL 字体文件）
- otto-neutral GIF 从现成 managed component 引用打包
- 其余字体/emoji/图标全部复用现有资源

## 5. 模块划分与接口

所有新增天气代码在 `main/weather/`，UI 代码在 `main/display/dashboard/`。

### 5.1 `WeatherService`（`main/weather/weather_service.h/.cc`）

单例，设备唯一天气数据入口，不持有 LVGL 对象。

- `void Start()`：WiFi 连接后启动内部状态机（可重复调用，幂等）
- `WeatherSnapshot GetSnapshot()`：返回只读数据快照
- `void SetUpdateCallback(std::function<void()> cb)`：数据更新时通知订阅者（UI）
- 内部循环：定位 → 取天气 → 取 AQI → 休眠 30 分钟；失败等 5 分钟；WiFi 状态通过已有事件回调感知（暂停/恢复）

`WeatherSnapshot` 字段：

```
bool valid;               // 是否曾成功获取
std::string city;         // 城市名
std::string weather_text; // 阴/晴…
std::string weather_icon; // 和风图标代码，映射到本地 emoji
int temperature;          // °C
int humidity;             // %
int aqi;                  // 空气指数
std::string aqi_category; // 优/良…
time_t updated_at;
```

### 5.2 `GeoIpLocator`（weather_service.cc 内私有实现）

- ESP HTTP client 请求 `http://ip-api.com/json/?lang=zh-CN&fields=status,city,lat,lon`
- JSON 解析使用仓库 cJSON 工具（`main/cjson_utils.h`）
- 5 秒超时；成功结果缓存 24 小时

### 5.3 `QWeatherClient`（weather_service.cc 内私有实现）

- HTTPS，host `devapi.qweather.com`（开发版 Key 使用该域名）
- `/v7/weather/now?location=lon,lat`、`/v7/air/now?location=lon,lat`
- 请求头携带 `X-QW-Api-Key` 或查询参数 `key`
- TLS 使用 ESP-IDF 公共根证书包（certificate bundle）
- 识别 API 业务错误码：`code` 非 `"200"` 视为失败；`401`/`403` 归类为密钥无效

JSON 解析为纯函数 `ParseWeatherNow()` / `ParseAirNow()`，输入响应字符串输出结构体，便于主机端单测。

### 5.4 `WeatherKeyStore`（`main/weather/weather_key_store.h/.cc`）

- NVS 命名空间 `weather`
- `esp_err_t SetKey(const std::string& key)` / `std::string GetKey()`
- Key 更新后通知 `WeatherService` 立即重试一次

MCP 工具注册（板子 `InitializeTools()` 中或 WeatherService 自注册）：

- 名称：`self.system.set_weather_api_key`
- 参数：`key`（字符串）
- 描述须提示：密钥保存于设备本机 NVS，用户需明确确认后再设置

### 5.5 `DashboardUI`（`main/display/dashboard/dashboard_ui.h/.cc`）

- 构造：`DashboardUI(lv_obj_t* parent)`，在 parent 下自建全屏容器，默认 hidden
- `void Show()` / `void Hide()`：淡入/淡出
- `void UpdateClock()`：每秒调用，内部读取系统时钟
- `void UpdateWeather(const WeatherSnapshot&)`：刷新天气区与温湿度条
- `void UpdateNetwork(const std::string& icon)`：刷新 WiFi 图标
- 纯映射函数（声明在头文件便于单测）：
  - `AqiLevel AqiToLevel(int aqi)`：分档与颜色
  - `int TempToPercent(int temp)`：-10~40°C 映射 0-100，钳制
  - 和风图标代码 → noto emoji 字符的映射表（晴/多云/阴/雨/雪/雾等常见类别）

### 5.6 接入点

- `LcdDisplay` 增加 `ShowDashboard()` / `HideDashboard()`；内部持有 `DashboardUI`（仅 `CONFIG_WEATHER_DASHBOARD` 时编译）
- `application.cc`：
  - idle 分支：`display->ShowDashboard()`
  - 离开 idle（其余状态分支或统一入口）：`display->HideDashboard()`
  - 心跳处理中：若仪表盘可见，调用 `UpdateClock()`
- `WeatherService::Start()` 在 WiFi 连接成功事件后启动
- 天气更新回调 → `LcdDisplay` → `DashboardUI::UpdateWeather()`
- 网络图标更新链路已存在（每 10 秒轮询 + 事件即时刷新），同步给仪表盘一份

### 5.7 构建配置

- `main/weather/` 加入 CMake 源文件列表
- Kconfig：`CONFIG_WEATHER_DASHBOARD`（bool，`default y if BOARD_TYPE_LCDWIKI_ES3C28P`，否则默认 n）
- 仪表盘 UI 与 WeatherService 仅在该选项开启时编译

## 6. 错误处理

| 情况 | 行为 |
|---|---|
| 未配置 API Key | 时钟/网络正常显示；天气区显示"天气未配置"；设置 Key 后自动生效 |
| 定位失败 | 日志记录，5 分钟后重试；定位未成功前不发天气请求 |
| 天气/AQI HTTP 失败 | 对应字段显示"--"，5 分钟重试 |
| API 返回 401/403 | 天气区提示"密钥无效"，日志记录 |
| JSON 缺字段/解析失败 | 等同请求失败处理，不崩溃 |
| 时间未同步 | 时钟显示 `--:--`，日期显示占位符 |
| WiFi 断开 | 暂停天气循环，图标置灰；重连后立即刷新 |

所有网络失败均不得影响聊天、音频主流程。

## 7. 测试策略

### 主机端单元测试

- `ParseWeatherNow()` / `ParseAirNow()`：成功响应、业务错误码、缺字段三种 fixture
- `AqiToLevel()`：覆盖 0/50/51/100/101/150/200/越界值
- `TempToPercent()`：-10、25、40、越界值
- 定位响应解析：成功/失败 status

### 硬件验证

- `idf.py build`（IDF v6.0.2，板子 `BOARD_TYPE_LCDWIKI_ES3C28P`）：0 error、0 warning
- 实机流程：
  1. 未配 Key 待机：提示文案正确
  2. 语音设置 Key：天气区出现真实数据
  3. 待机 → 对话 → 待机：界面切换、淡入淡出正常
  4. 断网/重连：图标与刷新恢复正常
  5. 时钟跨分钟/跨日期变化正确
