import os
import sys
import paramiko
import subprocess
import platform
from argparse import ArgumentParser

# 从环境变量获取远程服务器信息
REMOTE_SERVER = os.getenv('REMOTE_SERVER')
REMOTE_USER = os.getenv('REMOTE_USER')
REMOTE_PASS = os.getenv('REMOTE_PASS')

def check_env():
    if not REMOTE_SERVER or not REMOTE_USER or not REMOTE_PASS:
        print("请确保已设置 REMOTE_SERVER, REMOTE_USER, REMOTE_PASS等环境变量")
        sys.exit(1)

# 下载文件从远程服务器
def download_file_from_server(remote_file, local_file, force_update):
    if not force_update and not os.path.exists(local_file):
        print("本地文件不存在，强制拉取")
    elif not force_update and os.path.exists(local_file):
        print(f"使用本地暂存文件{local_file}")
        return
    
    print(f"正在下载{remote_file} 到 {local_file}")
    try:
        # 设置SSH客户端
        ssh = paramiko.SSHClient()
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh.connect(REMOTE_SERVER, username=REMOTE_USER, password=REMOTE_PASS)

        # 下载文件
        sftp = ssh.open_sftp()
        sftp.get(remote_file, local_file)
        sftp.close()
        print(f"文件下载成功")
    except Exception as e:
        print(f"文件下载失败: {e}")
    finally:
        ssh.close()

# 使用IDF的esptool烧录二进制文件
def burn_to_device(serial_port, baud_rate, file_path):
    file_path = os.path.normpath(file_path)

    # 设置命令
    esptool_command = f"python -m esptool -p {serial_port} -b {baud_rate} write_flash 0x0 {file_path}"
    print(f"启动烧录：{esptool_command}")

    # 调用esptool烧录
    exit_code = os.system(esptool_command)
    if exit_code:
        print(f"烧录失败: {exit_code}\n")
    else:
        print("烧录完成\n")

# 主函数
def main():
    # 解析命令行参数
    parser = ArgumentParser(description="拉取远程服务器编译生成的二进制文件到本地，通过串口烧录到目标硬件")
    parser.add_argument('-p', "--serial_port", type=str, help="串口设备路径（例如：COM3 或 /dev/ttyUSB0）", required=True)
    parser.add_argument('-b', "--baud_rate", type=int, help="波特率（例如：115200）", default=2000000)
    parser.add_argument('-r', "--remote_file", type=str, help="远程服务器上的二进制文件路径", default='/home/jimmy/work/repos/xiaozhi-esp32/build/merged-binary.bin')
    parser.add_argument('-l', "--local_file", type=str, help="本地保存的二进制文件路径", default='./merged-binary.bin')
    parser.add_argument('-f', "--force_update", type=bool, help="更新本地暂存文件", default=False)
    args = parser.parse_args()

    check_env()

    # 下载文件到本地
    download_file_from_server(args.remote_file, args.local_file, args.force_update)

    # 使用IDF环境烧录到目标硬件
    burn_to_device(args.serial_port, args.baud_rate, args.local_file)

if __name__ == '__main__':
    main()