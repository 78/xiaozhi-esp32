# 编译配置命令

**配置编译目标为 ESP32：**

```bash
idf.py set-target esp32
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> 面包板 ESP32 DevKit
```

**修改 flash 大小：**

```
Serial flasher config -> Flash size -> 4 MB
```

**修改分区表：**

```
Partition Table -> Custom partition CSV file -> partitions/v1/4m.csv
```

**编译：**

```bash
idf.py build
```