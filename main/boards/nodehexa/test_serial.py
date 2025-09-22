#!/usr/bin/env python3
"""
NodeHexa 串口通信测试脚本

用于测试六足机器人的串口控制功能
"""

import serial
import json
import time
import sys

class NodeHexaSerialTester:
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        
    def connect(self):
        """连接到串口"""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1,
                write_timeout=1
            )
            print(f"成功连接到 {self.port}")
            return True
        except serial.SerialException as e:
            print(f"连接失败: {e}")
            return False
    
    def disconnect(self):
        """断开串口连接"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("串口连接已断开")
    
    def send_command(self, movement_mode):
        """发送运动命令"""
        if not self.ser or not self.ser.is_open:
            print("串口未连接")
            return False
        
        # 将线性索引转换为位掩码
        if isinstance(movement_mode, int) and 0 <= movement_mode <= 12:
            actual_mode = 1 << movement_mode
        else:
            actual_mode = movement_mode
        
        command = {
            "movementMode": actual_mode
        }
        
        json_command = json.dumps(command) + '\n'
        print(f"发送命令: {json_command.strip()}")
        
        try:
            self.ser.write(json_command.encode('utf-8'))
            return True
        except serial.SerialException as e:
            print(f"发送失败: {e}")
            return False
    
    def read_response(self):
        """读取响应"""
        if not self.ser or not self.ser.is_open:
            return None
        
        try:
            if self.ser.in_waiting > 0:
                response = self.ser.readline().decode('utf-8').strip()
                if response:
                    print(f"收到响应: {response}")
                    return response
        except serial.SerialException as e:
            print(f"读取失败: {e}")
        
        return None
    
    def test_movement_commands(self):
        """测试所有运动命令"""
        movements = [
            (0, "待机"),
            (1, "前进"),
            (2, "快速前进"),
            (3, "后退"),
            (4, "左转"),
            (5, "右转"),
            (6, "左移"),
            (7, "右移"),
            (8, "攀爬"),
            (9, "X轴旋转"),
            (10, "Y轴旋转"),
            (11, "Z轴旋转"),
            (12, "扭动")
        ]
        
        print("\n=== 开始测试运动命令 ===")
        
        for mode, name in movements:
            print(f"\n测试: {name} (模式 {mode})")
            
            if self.send_command(mode):
                # 等待响应
                time.sleep(0.5)
                response = self.read_response()
                
                if response:
                    try:
                        resp_data = json.loads(response)
                        if resp_data.get("status") == "success":
                            print(f"✓ {name} 命令执行成功")
                        else:
                            print(f"✗ {name} 命令执行失败: {resp_data.get('message', '未知错误')}")
                    except json.JSONDecodeError:
                        print(f"✗ 响应格式错误: {response}")
                else:
                    print(f"? {name} 命令无响应")
            else:
                print(f"✗ {name} 命令发送失败")
            
            time.sleep(1)  # 等待运动执行
        
        print("\n=== 测试完成 ===")
    
    def interactive_mode(self):
        """交互模式"""
        print("\n=== 交互模式 ===")
        print("输入运动模式数字 (0-12) 或 'q' 退出:")
        print("0: 待机, 1: 前进, 2: 快速前进, 3: 后退")
        print("4: 左转, 5: 右转, 6: 左移, 7: 右移")
        print("8: 攀爬, 9: X轴旋转, 10: Y轴旋转, 11: Z轴旋转, 12: 扭动")
        
        while True:
            try:
                user_input = input("\n请输入命令: ").strip()
                
                if user_input.lower() == 'q':
                    break
                
                try:
                    mode = int(user_input)
                    if 0 <= mode <= 12:
                        self.send_command(mode)
                        time.sleep(0.5)
                        self.read_response()
                    else:
                        print("无效的运动模式，请输入 0-12 之间的数字")
                except ValueError:
                    print("请输入有效的数字")
                    
            except KeyboardInterrupt:
                print("\n退出交互模式")
                break

def main():
    # 检查命令行参数
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = '/dev/ttyUSB0'
    
    print(f"NodeHexa 串口通信测试")
    print(f"串口: {port}")
    print(f"波特率: 115200")
    
    tester = NodeHexaSerialTester(port)
    
    if not tester.connect():
        print("无法连接到串口，请检查:")
        print("1. 串口设备是否存在")
        print("2. 串口权限是否正确")
        print("3. 设备是否已连接")
        return
    
    try:
        # 等待设备初始化
        print("等待设备初始化...")
        time.sleep(2)
        
        # 清空缓冲区
        tester.ser.reset_input_buffer()
        tester.ser.reset_output_buffer()
        
        # 选择测试模式
        print("\n选择测试模式:")
        print("1. 自动测试所有运动命令")
        print("2. 交互模式")
        
        while True:
            choice = input("请选择 (1/2): ").strip()
            if choice == '1':
                tester.test_movement_commands()
                break
            elif choice == '2':
                tester.interactive_mode()
                break
            else:
                print("请输入 1 或 2")
    
    except KeyboardInterrupt:
        print("\n测试被中断")
    
    finally:
        tester.disconnect()

if __name__ == "__main__":
    main()
