# jiuchuang-xiaozhi-sound
九川科技小智音响

## 编译指南
**开发环境**：ESP-IDF 5.4.1

### 编译步骤：
1. 使用 VSCode 打开项目文件夹；
2. 清除工程；
3. 点击右下角提示生成 [compile_commands.json]；
4. 选择 ESP-IDF 版本为 `5.4.1`；
5. 选择设备目标为 [esp32s3]；
6. 点击 **SDK 配置编辑器**；
7. 设置 **Custom partition CSV file** 为 `partitions/v1/16m.csv`；
8. 设置 **Board Type** 为 **九川科技**；
9. 保存配置并编译。