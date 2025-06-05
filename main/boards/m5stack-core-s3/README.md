# 使用说明 


1. 设置编译目标为 esp32s3

```shell
idf.py set-target esp32s3
```

2. 修改配置 

```shell
cp main/boards/m5stack-core-s3/sdkconfig.cores3 sdkconfig
```

3. 编译烧录程序

```shell
idf.py build flash monitor
```

> [!NOTE]
> 进入下载模式：长按复位按键(约3秒)，直至内部指示灯亮绿色，松开按键。


 
