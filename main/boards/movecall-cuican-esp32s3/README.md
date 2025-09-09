# ESP32-S3 编译配置指南

## 基本命令

### 设置目标芯片

```bash
idf.py set-target esp32s3
```

### 打开配置界面：

```bash
idf.py menuconfig
```
### Flash 配置:

```
Serial flasher config -> Flash size -> 8 MB
```

### 分区表配置：

```
Partition Table -> Custom partition CSV file -> partitions/v2/8m.csv
```

### 开发板选择：

```
Xiaozhi Assistant -> Board Type -> Movecall CuiCan 璀璨·AI吊坠
```

### 启用编译优化：

```
Component config → Compiler options → Optimization Level → Optimize for size (-Os)
```

### 编译：

```bash
idf.py build
```