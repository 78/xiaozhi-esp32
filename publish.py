#! /usr/bin/env python3
from dotenv import load_dotenv
load_dotenv()

import os
import oss2
import json

def get_version():
    with open('CMakeLists.txt', 'r') as f:
        for line in f:
            if line.startswith('set(PROJECT_VER'):
                return line.split('"')[1]
    return '0.0.0'

def upload_bin_to_oss(bin_path, oss_key):
    auth = oss2.Auth(os.environ['OSS_ACCESS_KEY_ID'], os.environ['OSS_ACCESS_KEY_SECRET'])
    bucket = oss2.Bucket(auth, os.environ['OSS_ENDPOINT'], os.environ['OSS_BUCKET_NAME'])
    bucket.put_object(oss_key, open(bin_path, 'rb'))


if __name__ == '__main__':
    # 获取版本号
    version = get_version()
    print(f'version: {version}')

    # 上传 bin 文件到 OSS
    upload_bin_to_oss('build/xiaozhi.bin', f'firmwares/xiaozhi-{version}.bin')

    # File URL
    file_url = os.path.join(os.environ['OSS_BUCKET_URL'], f'firmwares/xiaozhi-{version}.bin')
    print(f'Uploaded bin to OSS: {file_url}')

    firmware_json = {
        "version": version,
        "url": file_url
    }
    with open(f"build/firmware.json", "w") as f:
        json.dump(firmware_json, f, indent=4)
    
    # copy firmware.json to server
    firmware_config_path = os.environ['FIRMWARE_CONFIG_PATH']
    ret = os.system(f'scp build/firmware.json {firmware_config_path}')
    if ret != 0:
        print(f'Failed to copy firmware.json to server')
        exit(1)
    print(f'Copied firmware.json to server: {firmware_config_path}')



    
