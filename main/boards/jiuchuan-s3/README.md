# jiuchuan-xiaozhi-sound
九川科技小智AI音箱

## 🛠️ 编译指南
**开发环境**：ESP-IDF v5.4.1

### 编译步骤：
> ⚠️ **提示**：若在编译过程中访问在线库失败，可以尝试切换加速器状态，或修改 [idf_component.yml] 文件，替换为国内镜像源。

1. 使用 VSCode 打开项目文件夹；
2. 清除工程（Clean Project）；
3. 设置 ESP-IDF 版本为 `v5.4.1`；
4. 点击 VSCode 右下角提示，生成 [compile_commands.json] 文件；
5. 设置目标设备为 `[esp32s3] -> [JTAG]`；
6. 打开 **SDK Configuration Editor**；
7. 设置 **Board Type** 为 **九川科技**；
8. 保存配置并开始编译。

## 🔌 烧录步骤
1. 使用数据线连接电脑与音箱；
2. 关闭设备电源后，长按电源键不松手；
3. 在烧录工具中选择对应的串口（COM Port）；
4. 点击烧录按钮，选择 UART 模式；
5. 烧录完成前请勿松开电源键。