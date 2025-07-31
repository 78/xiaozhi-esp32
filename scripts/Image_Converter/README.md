# LVGL图片转换工具  

这个目录包含两个用于处理和转换图片为LVGL格式的Python脚本：

## 1. LVGLImage (LVGLImage.py)

引用自LVGL[官方repo](https://github.com/lvgl/lvgl)的转换脚本[LVGLImage.py](https://github.com/lvgl/lvgl/blob/master/scripts/LVGLImage.py)  

## 2. LVGL图片转换工具 (lvgl_tools_gui.py)

调用`LVGLImage.py`，将图片批量转换为LVGL图片格式  
可用于修改小智的默认表情，具体修改教程[在这里](https://www.bilibili.com/video/BV12FQkYeEJ3/)

### 特性

- 图形化操作，界面更友好
- 支持批量转换图片
- 自动识别图片格式并选择最佳的颜色格式转换
- 多分辨率支持

### 使用方法

创建虚拟环境
```bash
# 创建 venv
python -m venv venv
# 激活环境
source venv/bin/activate  # Linux/Mac
venv\Scripts\activate      # Windows
```

安装依赖
```bash
pip install -r requirements.txt
```

运行转换工具

```bash
# 激活环境
source venv/bin/activate  # Linux/Mac
venv\Scripts\activate      # Windows
# 运行
python lvgl_tools_gui.py
```
