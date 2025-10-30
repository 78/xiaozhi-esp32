# Seeed Studio XIAO ESP32-S3
参照Bread-compact-wifi改造的适配XIAO ESP32-S3开发板的面包板版本。

## 🛠️ 编译指南
**开发环境**：ESP-IDF v5.4.1

### 编译步骤：
默认Windows已安装ESP-IDF环境
1. 双击打开ESP-IDF 5.5CMD桌面图标，并 cd 进入项目文件夹
2. 输入 idf.py fullclean, 回车
3. 输入 idf.py set-target esp32s3，回车
4. 输入 idf.py menuconfig
5. 进入 Serial flasher config -> Flash size -> 8MB
6. 进入 Partition Table -> Custom partition CSV file -> 复制粘贴此路径：partitions/v2/8m.csv
7. 进入 Xiaozhi Assistant -> Board Type -> Seeed Studio XIAO ESP32-S3
8. 进入 OLED Type -> SSD1306 128*32
8. 按S保存，Q退出。
9. 输入 idf.py build
10. 等待编译结束后开始烧录

## 🔌 烧录步骤
1. 连接开发板到电脑
2. Windows： 此电脑右键找到管理，进入设备管理器查看端口
3. 输入 idf.py -p COM(你的端口号) flash
4. 烧录完成后可以输入 idf.py -p COM(你的端口号) monitor 打开串口监视器查看开发板状态
5. 祝你玩儿的开心~~~