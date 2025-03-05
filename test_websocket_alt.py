import websocket
import json
import ssl
import time

# 启用调试输出
websocket.enableTrace(True)

# 连接配置 - 只保留必要的自定义头
url = "wss://www.u2416165.nyat.app:28103/"
headers = {
    # 不要包含这些头，库会自动添加：Host, Upgrade, Connection, Sec-WebSocket-Version
    "Authorization": "Bearer test-token"
}

# 创建WebSocket连接
ws = websocket.create_connection(
    url,
    header=headers,
    sslopt={"cert_reqs": ssl.CERT_NONE}
)

try:
    print("连接成功！")
    
    # 发送hello消息
    hello_msg = {"type": "hello", "version": 1, "transport": "websocket", 
                "audio_params": {"format": "opus", "sample_rate": 16000, "channels": 1, "frame_duration": 20}}
    ws.send(json.dumps(hello_msg))
    print(f"已发送hello消息")
    
    # 接收响应
    result = ws.recv()
    print(f"收到响应: {result}")
    
    # 发送二进制数据
    binary_data = bytes([1, 2, 3, 4, 5, 6, 7, 8])
    ws.send_binary(binary_data)
    print(f"已发送二进制数据: {len(binary_data)} 字节")
    
    # 等待响应
    ws.settimeout(5)
    try:
        result = ws.recv()
        print(f"收到响应: {result}")
    except websocket.WebSocketTimeoutException:
        print("等待响应超时")
    
finally:
    ws.close()
    print("连接已关闭") 