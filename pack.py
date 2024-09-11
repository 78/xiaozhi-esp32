#! /usr/bin/env python3

import csv
import os

# 例如：1000, 0x1000, 1M
def read_value(text):
    text = text.strip()
    if text.endswith('K'):
        return int(text[:-1]) * 1024
    elif text.endswith('M'):
        return int(text[:-1]) * 1024 * 1024
    else:
        if text.startswith('0x'):
            return int(text, 16)
        else:
            return int(text)


def write_bin(image_data, offset, file_path, max_size=None):
    # Read file_path and write to image_data
    with open(file_path, 'rb') as f:
        data = f.read()
        if max_size is not None:
            assert len(data) <= max_size, f"Data from {file_path} is too large"
        image_data[offset:offset+len(data)] = data
        print(f"Write {os.path.basename(file_path)} to 0x{offset:08X} with size 0x{len(data):08X}")


'''
根据 partitions.csv 文件，把 bin 文件打包成一个 4MB 的 image 文件，方便烧录
'''
def pack_firmware_image():
    # Create a 4MB image filled with 0xFF
    image_size = 4 * 1024 * 1024
    image_data = bytearray([0xFF] * image_size)

    build_dir = os.path.join(os.path.dirname(__file__), 'build')
    write_bin(image_data, 0, os.path.join(build_dir, 'bootloader', 'bootloader.bin'))
    write_bin(image_data, 0x8000, os.path.join(build_dir, 'partition_table', 'partition-table.bin'))

    # 读取 partitions.csv 文件
    with open('partitions.csv', 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            if row[0] == 'model':   
                file_path = os.path.join(build_dir, 'srmodels', 'srmodels.bin')
            elif row[0] == 'factory':
                file_path = os.path.join(build_dir, 'xiaozhi.bin')
            else:
                continue

            offset = read_value(row[3])
            size = read_value(row[4])
            write_bin(image_data, offset, file_path, size)

    # 写入 image 文件
    output_path = os.path.join(build_dir, 'xiaozhi.img')
    with open(output_path, 'wb') as f:
        f.write(image_data)
    print(f"Image file {output_path} created with size 0x{len(image_data):08X}")

    # Compress image with zip without directory
    os.system(f"zip -j {output_path}.zip {output_path}")


if __name__ == '__main__':
    pack_firmware_image()
