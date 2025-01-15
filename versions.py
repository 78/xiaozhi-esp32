#! /usr/bin/env python3
from dotenv import load_dotenv
load_dotenv()

import os
import struct
import zipfile
import oss2
import json

def get_chip_id_string(chip_id):
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
    if basename.startswith("v0.7") or basename.startswith("v0.8") or basename.startswith("v0.9"):
        return basename.split("_")[1]
    raise Exception(f"Unknown board name: {basename}")

def read_binary(dir_path):
    merged_bin_path = os.path.join(dir_path, "merged-binary.bin")
    data = open(merged_bin_path, "rb").read()[0x200000:]
    if data[0] != 0xE9:
        print(dir_path, "is not a valid image")
        return
    # get flash size
    flash_size = get_flash_size(data[0x3] >> 4)
    chip_id = get_chip_id_string(data[0xC])
    # get segments
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
    
    # extract bin file
    bin_path = os.path.join(dir_path, "xiaozhi.bin")
    if not os.path.exists(bin_path):
        print("extract bin file to", bin_path)
        open(bin_path, "wb").write(data)

    # The app desc is in the first segment
    desc = get_app_desc(segments[0])
    return {
        "chip_id": chip_id,
        "flash_size": flash_size,
        "board": get_board_name(dir_path),
        "application": desc,
    }

def extract_zip(zip_path, extract_path):
    if not os.path.exists(extract_path):
        os.makedirs(extract_path)
    print(f"Extracting {zip_path} to {extract_path}")
    with zipfile.ZipFile(zip_path, 'r') as zip_ref:
        zip_ref.extractall(extract_path)

def upload_dir_to_oss(source_dir, target_dir):
    auth = oss2.Auth(os.environ['OSS_ACCESS_KEY_ID'], os.environ['OSS_ACCESS_KEY_SECRET'])
    bucket = oss2.Bucket(auth, os.environ['OSS_ENDPOINT'], os.environ['OSS_BUCKET_NAME'])
    for filename in os.listdir(source_dir):
        oss_key = os.path.join(target_dir, filename)
        print('uploading', oss_key)
        bucket.put_object(oss_key, open(os.path.join(source_dir, filename), 'rb'))

def main():
    release_dir = "releases"
    versions = []
    # look for zip files startswith "v"
    for name in os.listdir(release_dir):
        if name.startswith("v") and name.endswith(".zip"):
            tag = name[:-4]
            folder = os.path.join(release_dir, tag)
            if not os.path.exists(folder):
                os.makedirs(folder)
                extract_zip(os.path.join(release_dir, name), folder)
                info = read_binary(folder)
                target_dir = os.path.join("firmwares", tag)
                info["tag"] = tag
                info["url"] = os.path.join(os.environ['OSS_BUCKET_URL'], target_dir, "xiaozhi.bin")
                open(os.path.join(folder, "info.json"), "w").write(json.dumps(info, indent=4))
                # upload all file to oss
                upload_dir_to_oss(folder, target_dir)
            # read info.json
            info = json.load(open(os.path.join(folder, "info.json")))
            versions.append(info)

    # sort versions by version
    versions.sort(key=lambda x: x["tag"], reverse=True)
    # write versions to file
    versions_path = os.path.join(release_dir, "versions.json")
    open(versions_path, "w").write(json.dumps(versions, indent=4))
    print(f"Versions written to {versions_path}")

    # copy versions.json to server
    versions_config_path = os.environ.get('VERSIONS_CONFIG_PATH')
    if not versions_config_path:
        print("VERSIONS_CONFIG_PATH is not set")
        exit(1)
    ret = os.system(f'scp {versions_path} {versions_config_path}')
    if ret != 0:
        print(f'Failed to copy versions.json to server')
        exit(1)
    print(f'Copied versions.json to server: {versions_config_path}')



if __name__ == "__main__":
    main()