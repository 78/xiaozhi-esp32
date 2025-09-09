#!/usr/bin/env python3
"""
音频实时监听与绘图系统主程序
基于Qt GUI + Matplotlib + UDP接收 + AFSK解码字符串
"""

import sys
import asyncio
from graphic import main

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("程序被用户中断")
    except Exception as e:
        print(f"程序执行出错: {e}")
        sys.exit(1)
