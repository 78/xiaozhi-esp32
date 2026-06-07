import socket
import struct
import numpy as np
import cv2

# --- 配置参数 ---
HOST = '0.0.0.0'       # 监听所有网卡
PORT = 12345           # 端口号
WIDTH = 320
HEIGHT = 320
CHANNELS = 3           # RGB888

def recvall(sock, count):
    """
    辅助函数：确保接收到指定数量的字节
    TCP 是流式传输，sock.recv(count) 可能会返回少于 count 的数据
    """
    buf = b''
    while count:
        try:
            newbuf = sock.recv(count)
            if not newbuf: return None
            buf += newbuf
            count -= len(newbuf)
        except Exception as e:
            print(f"Receive error: {e}")
            return None
    return buf

def main():
    # 1. 创建 TCP Socket
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server_sock.bind((HOST, PORT))
        server_sock.listen(1)
        print(f"TCP 服务端已启动，监听 {HOST}:{PORT}")
        print("等待 ESP32 连接...")
    except Exception as e:
        print(f"无法绑定端口: {e}")
        return

    conn, addr = server_sock.accept()
    print(f"ESP32 已连接: {addr}")

    while True:
        try:
            # 2. 协议解析：先接收 4 字节的头部（表示图像数据长度）
            # 'I' 表示 unsigned int (4 bytes)
            header_data = recvall(conn, 4)
            if not header_data:
                print("连接断开")
                break
                
            # 解析出数据长度
            data_len = struct.unpack('<I', header_data)[0]
            
            # 3. 接收图像实体数据
            img_data = recvall(conn, data_len)
            if not img_data:
                break

            # 4. 图像转换与显示
            # 将字节转为 numpy 数组
            frame_np = np.frombuffer(img_data, dtype=np.uint8)
            
            # 校验数据长度是否符合预期（800*800*3）
            expected_len = WIDTH * HEIGHT * CHANNELS
            if len(frame_np) != expected_len:
                print(f"警告：接收数据长度 ({len(frame_np)}) 与预期 ({expected_len}) 不符，跳过此帧")
                continue

            # Reshape
            frame = frame_np.reshape((HEIGHT, WIDTH, CHANNELS))
            
            # RGB 转 BGR (OpenCV 使用 BGR)
            frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)

            cv2.imshow("ESP32 TCP Stream", frame)
            
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

        except Exception as e:
            print(f"发生错误: {e}")
            break

    conn.close()
    server_sock.close()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()