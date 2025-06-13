
产品链接:https://www.displaysmodule.com/sale-51877354-86-5-86-5-37-8-mm-2-8-inches-jc1060wp470n-c-i-w-module-size-wall-plate-esp32-p4.html

如果烧录完成之后出现屏幕一直不亮的情况请查看串口输出，是否有如下报错

```c
I (742) jd9165: version: 1.0.2
I (745) gpio: GPIO[27]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
E (10902) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
E (10902) task_wdt:  - IDLE0 (CPU 0)
E (10902) task_wdt: Tasks currently running:
E (10902) task_wdt: CPU 0: main
E (10902) task_wdt: CPU 1: IDLE1
E (10902) task_wdt: Print CPU 0 (current core) backtrace

```

如果出现此报错请修改.lane_bit_rate_mbps的值，尝试使用550，750或900

