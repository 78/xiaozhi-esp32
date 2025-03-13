#! /usr/bin/env python3
from dotenv import load_dotenv
load_dotenv()  # 加载环境变量文件 .env

import os
import struct
import zipfile
import oss2  # 阿里云 OSS SDK
import json
import requests
from requests.exceptions import RequestException

# 切换到项目根目录
os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def get_chip_id_string(chip_id):
    """
    根据芯片 ID 获取芯片名称。
    参数:
    - chip_id: 芯片 ID
    返回值: 芯片名称字符串
    """
    return {
        0x0000: "esp32",
        0x0002: "esp32s2",
        0x0005: "esp32c3",
        0x0009: "esp32s3",
        0x000C: "esp32c2",
        0x000D: "esp32c6",
        0x0010: "esp32h2",
        0x0011: "esp32c5",
        0x0012: "esp32p4",
        0x0017: "esp32c5",
    }[chip_id]

def get_flash_size(flash_size):
    """
    根据 Flash 大小编码获取实际 Flash 大小。
    参数:
    - flash_size: Flash 大小编码
    返回值: Flash 大小（字节数）
    """
    MB = 1024 * 1024
    return {
        0x00: 1 * MB,
        0x01: 2 * MB,
        0x02: 4 * MB,
        0x03: 8 * MB,
        0x04: 16 * MB,
        0x05: 32 * MB,
        0x06: 64 * MB,
        0x07: 128 * MB,
    }[flash_size]

def get_app_desc(data):
    """
    从二进制数据中提取应用程序描述信息。
    参数:
    - data: 二进制数据
    返回值: 包含应用程序描述信息的字典
    """
    magic = struct.unpack("<I", data[0x00:0x04])[0]
    if magic != 0xabcd5432:
        raise Exception("Invalid app desc magic")
    version = data[0x10:0x30].decode("utf-8").strip('\0')
    project_name = data[0x30:0x50].decode("utf-8").strip('\0')
    time = data[0x50:0x60].decode("utf-8").strip('\0')
    date = data[0x60:0x70].decode("utf-8").strip('\0')
    idf_ver = data[0x70:0x90].decode("utf-8").strip('\0')
    elf_sha256 = data[0x90:0xb0].hex()
    return {
        "name": project_name,
        "version": version,
        "compile_time": date + "T" + time,
        "idf_version": idf_ver,
        "elf_sha256": elf_sha256,
    }

def get_board_name(folder):
    """
    根据文件夹名称获取板子名称。
    参数:
    - folder: 文件夹路径
    返回值: 板子名称字符串
    """
    basename = os.path.basename(folder)
    if basename.startswith("v0.2"):
        return "bread-simple"
    if basename.startswith("v0.3") or basename.startswith("v0.4") or basename.startswith("v0.5") or basename.startswith("v0.6"):
        if "ML307" in basename:
            return "bread-compact-ml307"
        elif "WiFi" in basename:
            return "bread-compact-wifi"
        elif "KevinBox1" in basename:
            return "kevin-box-1"
    if basename.startswith("v0.7") or basename.startswith("v0.8") or basename.startswith("v0.9") or basename.startswith("v1."):
        return basename.split("_")[1]
    raise Exception(f"Unknown board name: {basename}")

def read_binary(dir_path):
    """
    读取二进制文件并提取固件信息。
    参数:
    - dir_path: 文件夹路径
    返回值: 包含固件信息的字典
    """
    merged_bin_path = os.path.join(dir_path, "merged-binary.bin")
    data = open(merged_bin_path, "rb").read()[0x100000:]
    if data[0] != 0xE9:
        print(dir_path, "is not a valid image")
        return
    # 获取 Flash 大小
    flash_size = get_flash_size(data[0x3] >> 4)
    chip_id = get_chip_id_string(data[0xC])
    # 获取段信息
    segment_count = data[0x1]
    segments = []
    offset = 0x18
    for i in range(segment_count):
        segment_size = struct.unpack("<I", data[offset + 4:offset + 8])[0]
        offset += 8
        segment_data = data[offset:offset + segment_size]
        offset += segment_size
        segments.append(segment_data)
    assert offset < len(data), "offset is out of bounds"
    
    # 提取 bin 文件
    bin_path = os.path.join(dir_path, "xiaozhi.bin")
    if not os.path.exists(bin_path):
        print("extract bin file to", bin_path)
        open(bin_path, "wb").write(data)

    # 应用程序描述信息在第一个段中
    desc = get_app_desc(segments[0])
    return {
        "chip_id": chip_id,
        "flash_size": flash_size,
        "board": get_board_name(dir_path),
        "application": desc,
        "firmware_size": len(data),
    }

def extract_zip(zip_path, extract_path):
    """
    解压 ZIP 文件到指定目录。
    参数:
    - zip_path: ZIP 文件路径
    - extract_path: 解压目标路径
    """
    if not os.path.exists(extract_path):
        os.makedirs(extract_path)
    print(f"Extracting {zip_path} to {extract_path}")
    with zipfile.ZipFile(zip_path, 'r') as zip_ref:
        zip_ref.extractall(extract_path)

def upload_dir_to_oss(source_dir, target_dir):
    """
    将目录中的文件上传到阿里云 OSS。
    参数:
    - source_dir: 本地目录路径
    - target_dir: OSS 目标目录路径
    """
    auth = oss2.Auth(os.environ['OSS_ACCESS_KEY_ID'], os.environ['OSS_ACCESS_KEY_SECRET'])
    bucket = oss2.Bucket(auth, os.environ['OSS_ENDPOINT'], os.environ['OSS_BUCKET_NAME'])
    for filename in os.listdir(source_dir):
        oss_key = os.path.join(target_dir, filename)
        print('uploading', oss_key)
        bucket.put_object(oss_key, open(os.path.join(source_dir, filename), 'rb'))

def post_info_to_server(info):
    """
    将固件信息发送到服务器。
    参数:
    - info: 包含固件信息的字典
    """
    try:
        # 从环境变量获取服务器 URL 和 token
        server_url = os.environ.get('VERSIONS_SERVER_URL')
        server_token = os.environ.get('VERSIONS_TOKEN')
        
        if not server_url or not server_token:
            raise Exception("Missing SERVER_URL or TOKEN in environment variables")

        # 准备请求头和数据
        headers = {
            'Authorization': f'Bearer {server_token}',
            'Content-Type': 'application/json'
        }
        
        # 发送 POST 请求
        response = requests.post(
            server_url,
            headers=headers,
            json={'jsonData': json.dumps(info)}
        )
        
        # 检查响应状态
        response.raise_for_status()
        
        print(f"Successfully uploaded version info for tag: {info['tag']}")
        
    except RequestException as e:
        if hasattr(e.response, 'json'):
            error_msg = e.response.json().get('error', str(e))
        else:
            error_msg = str(e)
        print(f"Failed to upload version info: {error_msg}")
        raise
    except Exception as e:
        print(f"Error uploading version info: {str(e)}")
        raise

def main():
    """
    主函数，处理固件发布流程。
    """
    release_dir = "releases"
    # 查找以 "v" 开头并以 ".zip" 结尾的文件
    for name in os.listdir(release_dir):
        if name.startswith("v") and name.endswith(".zip"):
            tag = name[:-4]
            folder = os.path.join(release_dir, tag)
            info_path = os.path.join(folder, "info.json")
            if not os.path.exists(info_path):
                if not os.path.exists(folder):
                    os.makedirs(folder)
                    extract_zip(os.path.join(release_dir, name), folder)
                info = read_binary(folder)
                target_dir = os.path.join("firmwares", tag)
                info["tag"] = tag
                info["url"] = os.path.join(os.environ['OSS_BUCKET_URL'], target_dir, "xiaozhi.bin")
                open(info_path, "w").write(json.dumps(info, indent=4))
                # 上传所有文件到 OSS
                upload_dir_to_oss(folder, target_dir)
                # 读取 info.json
                info = json.load(open(info_path))
                # 将 info.json 发送到服务器
                post_info_to_server(info)

if __name__ == "__main__":
    main()