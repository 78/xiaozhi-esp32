import sys
import json

# 读取 MCP 初始化请求
_ = json.load(sys.stdin)

# 输出 MCP 响应（标准 JSON）
json.dump({
    "type": "text",
    "text": "Hello World from MCP!"
}, sys.stdout)