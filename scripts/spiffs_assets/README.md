# SPIFFS Assets Builder

这个脚本用于构建 ESP32 项目的 SPIFFS 资源分区，将各种资源文件打包成可在设备上使用的格式。

## 功能特性

- 处理唤醒网络模型 (WakeNet Model)
- 集成文本字体文件
- 处理表情符号图片集合
- 自动生成资源索引文件
- 打包生成最终的 `assets.bin` 文件

## 依赖要求

- Python 3.6+
- 相关资源文件

## 使用方法

### 基本语法

```bash
./build.py --wakenet_model <wakenet_model_dir> \
    --text_font <text_font_file> \
    --emoji_collection <emoji_collection_dir>
```

### 参数说明

| 参数 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `--wakenet_model` | 目录路径 | 否 | 唤醒网络模型目录路径 |
| `--text_font` | 文件路径 | 否 | 文本字体文件路径 |
| `--emoji_collection` | 目录路径 | 否 | 表情符号图片集合目录路径 |

### 使用示例

```bash
# 完整参数示例
./build.py \
    --wakenet_model ../../managed_components/espressif__esp-sr/model/wakenet_model/wn9_nihaoxiaozhi_tts \
    --text_font ../../components/xiaozhi-fonts/build/font_puhui_common_20_4.bin \
    --emoji_collection ../../components/xiaozhi-fonts/build/emojis_64/

# 仅处理字体文件
./build.py --text_font ../../components/xiaozhi-fonts/build/font_puhui_common_20_4.bin

# 仅处理表情符号
./build.py --emoji_collection ../../components/xiaozhi-fonts/build/emojis_64/
```

## 工作流程

1. **创建构建目录结构**
   - `build/` - 主构建目录
   - `build/assets/` - 资源文件目录
   - `build/output/` - 输出文件目录

2. **处理唤醒网络模型**
   - 复制模型文件到构建目录
   - 使用 `pack_model.py` 生成 `srmodels.bin`
   - 将生成的模型文件复制到资源目录

3. **处理文本字体**
   - 复制字体文件到资源目录
   - 支持 `.bin` 格式的字体文件

4. **处理表情符号集合**
   - 扫描指定目录中的图片文件
   - 支持 `.png` 和 `.gif` 格式
   - 自动生成表情符号索引

5. **生成配置文件**
   - `index.json` - 资源索引文件
   - `config.json` - 构建配置文件

6. **打包最终资源**
   - 使用 `spiffs_assets_gen.py` 生成 `assets.bin`
   - 复制到构建根目录

## 输出文件

构建完成后，会在 `build/` 目录下生成以下文件：

- `assets/` - 所有资源文件
- `assets.bin` - 最终的 SPIFFS 资源文件
- `config.json` - 构建配置
- `output/` - 中间输出文件

## 支持的资源格式

- **模型文件**: `.bin` (通过 pack_model.py 处理)
- **字体文件**: `.bin`
- **图片文件**: `.png`, `.gif`
- **配置文件**: `.json`

## 错误处理

脚本包含完善的错误处理机制：

- 检查源文件/目录是否存在
- 验证子进程执行结果
- 提供详细的错误信息和警告

## 注意事项

1. 确保所有依赖的 Python 脚本都在同一目录下
2. 资源文件路径使用绝对路径或相对于脚本目录的路径
3. 构建过程会清理之前的构建文件
4. 生成的 `assets.bin` 文件大小受 SPIFFS 分区大小限制
