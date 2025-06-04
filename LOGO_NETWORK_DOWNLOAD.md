# Logo图片网络下载功能（独立版本管理）

## 概述
此修改将logo图片从本地编译时包含的方式改为从网络动态下载的方式，并实现了与动画图片独立的版本管理系统。

## 主要修改

### 1. 移除本地logo.h文件
- 删除了 `main/boards/moon/logo.h` 文件
- 移除了 `abrobot-1.28tft-wifi.cc` 中对 `logo.h` 的包含

### 2. 扩展图片资源管理器 - 独立版本管理
- 在 `ImageResourceManager` 中增加了logo图片的独立版本管理功能
- 新增方法：
  - `GetLogoImage()`: 获取logo图片数据
  - `LoadLogoFile()`: 加载logo文件
  - `CheckAndUpdateLogo()`: 检查并更新logo图片（独立版本管理）
  - `ReadLocalLogoVersion()`: 读取本地logo版本
  - `CheckServerLogoVersion()`: 检查服务器logo版本
  - `SaveLogoVersion()`: 保存logo版本
- 独立的版本管理：
  - 动画图片版本文件：`/resources/version.json`
  - Logo图片版本文件：`/resources/logo_version.json`

### 3. 修改IoT图片显示控制
- 移除了对本地logo.h的依赖
- 修改为从 `ImageResourceManager` 获取logo图片
- 在图片下载完成后自动更新logo图片

### 4. 更新图片轮播任务 - 独立检查更新
- 在任务启动时从资源管理器获取logo图片
- 分别检查动画图片和logo图片的版本更新
- 支持独立更新：logo可以独立于动画图片进行更新
- 保持静态模式和动画模式的切换功能

## 服务器端要求

### 文件结构
服务器需要提供以下文件：
- `logo.h`: logo图片文件
- `output_0001.h`: 动画第一帧
- `output_0002.h`: 动画第二帧
- `version`: 动画图片版本信息文件
- `logo_version`: logo图片版本信息文件（新增）

### API接口
- 动画图片资源：`http://your-server/images/`
- 动画图片版本：`http://your-server/images/version`
- Logo图片版本：`http://your-server/images/logo_version` （新增）

### 版本文件格式
两个版本文件都采用相同的JSON格式：
```json
{
  "version": "1.0.0"
}
```

## 使用方式
1. 设备启动时会分别检查本地是否有动画图片和logo图片
2. WiFi连接后，会自动检查服务器上的两个版本文件
3. 独立下载更新：
   - 如果动画图片有新版本，只下载动画图片
   - 如果logo有新版本，只下载logo图片
   - 两者可以独立更新，互不影响
4. 下载完成后会自动更新显示
5. 用户可以通过IoT控制切换静态logo模式和动画模式

## 优势
- ✨ **独立版本管理**：logo和动画图片可以独立更新
- 🎯 **精细化控制**：无需重新下载所有图片就能更新logo
- 📦 **减少网络传输**：只下载需要更新的图片
- 🔄 **向下兼容**：保持所有原有功能
- 🛡️ **错误隔离**：一个图片下载失败不影响另一个
- ⚡ **更快的更新**：logo更新更迅速

## 版本管理逻辑
1. **启动检查**：分别检查动画图片和logo图片的本地版本
2. **网络检查**：连接WiFi后，分别检查两个服务器版本
3. **独立更新**：
   - 动画图片版本更新 → 只下载动画图片
   - Logo版本更新 → 只下载logo图片
   - 都有更新 → 分别下载
   - 都是最新 → 跳过下载
4. **状态同步**：确保内存中的图片数据与最新版本同步 