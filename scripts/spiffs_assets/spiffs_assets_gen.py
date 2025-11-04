# SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import io
import os
import argparse
import json
import shutil
import math
import sys
import time
import numpy as np
import importlib
import subprocess
import urllib.request

from PIL import Image
from datetime import datetime
from dataclasses import dataclass
from typing import List
from pathlib import Path
from packaging import version

sys.dont_write_bytecode = True

GREEN = '\033[1;32m'
RED = '\033[1;31m'
RESET = '\033[0m'

@dataclass
class AssetCopyConfig:
    assets_path: str
    target_path: str
    spng_enable: bool
    sjpg_enable: bool
    qoi_enable: bool
    sqoi_enable: bool
    row_enable: bool
    support_format: List[str]
    split_height: int

@dataclass
class PackModelsConfig:
    target_path: str
    include_path: str
    image_file: str
    assets_path: str
    name_length: int

def generate_header_filename(path):
    asset_name = os.path.basename(path)

    header_filename = f'mmap_generate_{asset_name}.h'
    return header_filename

def compute_checksum(data):
    checksum = sum(data) & 0xFFFF
    return checksum

def sort_key(filename):
    basename, extension = os.path.splitext(filename)
    return extension, basename

def download_v8_script(convert_path):
    """
    Ensure that the lvgl_image_converter repository is present at the specified path.
    If not, clone the repository. Then, checkout to a specific commit.

    Parameters:
    - convert_path (str): The directory path where lvgl_image_converter should be located.
    """

    # Check if convert_path is not empty
    if convert_path:
        # If the directory does not exist, create it and clone the repository
        if not os.path.exists(convert_path):
            os.makedirs(convert_path, exist_ok=True)
            try:
                subprocess.run(
                    ['git', 'clone', 'https://github.com/W-Mai/lvgl_image_converter.git', convert_path],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    check=True
                )
            except subprocess.CalledProcessError as e:
                print(f'Git clone failed: {e}')
                sys.exit(1)

            # Checkout to the specific commit
            try:
                subprocess.run(
                    ['git', 'checkout', '9174634e9dcc1b21a63668969406897aad650f35'],
                    cwd=convert_path,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    check=True
                )
            except subprocess.CalledProcessError as e:
                print(f'Failed to checkout to the specific commit: {e}')
                sys.exit(1)
    else:
        print('Error: convert_path is NULL')
        sys.exit(1)

def download_v9_script(url: str, destination: str) -> None:
    """
    Download a Python script from a URL to a local destination.

    Parameters:
    - url (str): URL to download the script from.
    - destination (str): Local path to save the downloaded script.

    Raises:
    - Exception: If the download fails.
    """
    file_path = Path(destination)

    # Check if the file already exists
    if file_path.exists():
        if file_path.is_file():
            return

    try:
        # Create the parent directories if they do not exist
        file_path.parent.mkdir(parents=True, exist_ok=True)

        # Open the URL and retrieve the data
        with urllib.request.urlopen(url) as response, open(file_path, 'wb') as out_file:
            data = response.read()  # Read the entire response
            out_file.write(data)    # Write data to the local file

    except urllib.error.HTTPError as e:
        print(f'HTTP Error: {e.code} - {e.reason} when accessing {url}')
        sys.exit(1)
    except urllib.error.URLError as e:
        print(f'URL Error: {e.reason} when accessing {url}')
        sys.exit(1)
    except Exception as e:
        print(f'An unexpected error occurred: {e}')
        sys.exit(1)

def split_image(im, block_size, input_dir, ext, convert_to_qoi):
    """Splits the image into blocks based on the block size."""
    width, height = im.size

    if block_size:
        splits = math.ceil(height / block_size)
    else:
        splits = 1

    for i in range(splits):
        if i < splits - 1:
            crop = im.crop((0, i * block_size, width, (i + 1) * block_size))
        else:
            crop = im.crop((0, i * block_size, width, height))

        output_path = os.path.join(input_dir, str(i) + ext)
        crop.save(output_path, quality=100)

        qoi_module = importlib.import_module('qoi-conv.qoi')
        Qoi = qoi_module.Qoi
        replace_extension = qoi_module.replace_extension

        if convert_to_qoi:
            with Image.open(output_path) as img:
                if img.mode != 'RGBA':
                    img = img.convert('RGBA')

                img_data = np.asarray(img)
                out_path = qoi_module.replace_extension(output_path, 'qoi')
                new_image = qoi_module.Qoi().save(out_path, img_data)
                os.remove(output_path)


    return width, height, splits

def create_header(width, height, splits, split_height, lenbuf, ext):
    """Creates the header for the output file based on the format."""
    header = bytearray()

    if ext.lower() == '.jpg':
        header += bytearray('_SJPG__'.encode('UTF-8'))
    elif ext.lower() == '.png':
        header += bytearray('_SPNG__'.encode('UTF-8'))
    elif ext.lower() == '.qoi':
        header += bytearray('_SQOI__'.encode('UTF-8'))

    # 6 BYTES VERSION
    header += bytearray(('\x00V1.00\x00').encode('UTF-8'))

    # WIDTH 2 BYTES
    header += width.to_bytes(2, byteorder='little')

    # HEIGHT 2 BYTES
    header += height.to_bytes(2, byteorder='little')

    # NUMBER OF ITEMS 2 BYTES
    header += splits.to_bytes(2, byteorder='little')

    # SPLIT HEIGHT 2 BYTES
    header += split_height.to_bytes(2, byteorder='little')

    for item_len in lenbuf:
        # LENGTH 2 BYTES
        header += item_len.to_bytes(2, byteorder='little')

    return header

def save_image(output_file_path, header, split_data):
    """Saves the image with the constructed header and split data."""
    with open(output_file_path, 'wb') as f:
        if header is not None:
            f.write(header + split_data)
        else:
            f.write(split_data)

def handle_lvgl_version_v9(input_file: str, input_dir: str,
                                input_filename: str, convert_path: str) -> None:
    """
    Handle conversion for LVGL versions greater than 9.0.

    Parameters:
    - input_file (str): Path to the input image file.
    - input_dir (str): Directory of the input file.
    - input_filename (str): Name of the input file.
    - convert_path (str): Path for conversion scripts and outputs.
    """

    convert_file = os.path.join(convert_path, 'LVGLImage.py')
    lvgl_image_url = 'https://raw.githubusercontent.com/lvgl/lvgl/master/scripts/LVGLImage.py'

    download_v9_script(url=lvgl_image_url, destination=convert_file)
    lvgl_script = Path(convert_file)

    cmd = [
        'python',
        str(lvgl_script),
        '--ofmt', 'BIN',
        '--cf', config_data['support_raw_cf'],
        '--compress', 'NONE',
        '--output', str(input_dir),
        input_file
    ]

    try:
        result = subprocess.run(
            cmd,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        print(f'Completed {input_filename} -> BIN')
    except subprocess.CalledProcessError as e:
        print('An error occurred while executing LVGLImage.py:')
        print(e.stderr)
        sys.exit(e.returncode)

def handle_lvgl_version_v8(input_file: str, input_dir: str, input_filename: str, convert_path: str) -> None:
    """
    Handle conversion for supported LVGL versions (<= 9.0).

    Parameters:
    - input_file (str): Path to the input image file.
    - input_dir (str): Directory of the input file.
    - input_filename (str): Name of the input file.
    - convert_path (str): Path for conversion scripts and outputs.
    """

    download_v8_script(convert_path=convert_path)

    if convert_path not in sys.path:
        sys.path.append(convert_path)

    try:
        import lv_img_conv
    except ImportError as e:
        print(f"Failed to import 'lv_img_conv' from '{convert_path}': {e}")
        sys.exit(1)

    try:
        lv_img_conv.conv_one_file(
            root=Path(input_dir),
            filepath=Path(input_file),
            f=config_data['support_raw_ff'],
            cf=config_data['support_raw_cf'],
            ff='BIN',
            dither=config_data['support_raw_dither'],
            bgr_mode=config_data['support_raw_bgr'],
        )
        print(f'Completed {input_filename} -> BIN')
    except KeyError as e:
        print(f'Missing configuration key: {e}')
        sys.exit(1)
    except Exception as e:
        print(f'An error occurred during conversion: {e}')
        sys.exit(1)

def process_image(input_file, height_str, output_extension, convert_to_qoi=False):
    """Main function to process the image and save it as .sjpg, .spng, or .sqoi."""
    try:
        SPLIT_HEIGHT = int(height_str)
        if SPLIT_HEIGHT < 0:
            raise ValueError('Height must be a positive integer')
    except ValueError as e:
        print('Error: Height must be a positive integer')
        sys.exit(1)

    input_dir, input_filename = os.path.split(input_file)
    base_filename, ext = os.path.splitext(input_filename)
    OUTPUT_FILE_NAME = base_filename

    try:
        im = Image.open(input_file)
    except Exception as e:
        print('Error:', e)
        sys.exit(0)

    width, height, splits = split_image(im, SPLIT_HEIGHT, input_dir, ext, convert_to_qoi)

    split_data = bytearray()
    lenbuf = []

    if convert_to_qoi:
        ext = '.qoi'

    for i in range(splits):
        with open(os.path.join(input_dir, str(i) + ext), 'rb') as f:
            a = f.read()
        split_data += a
        lenbuf.append(len(a))
        os.remove(os.path.join(input_dir, str(i) + ext))

    header = None
    if splits == 1 and convert_to_qoi:
        output_file_path = os.path.join(input_dir, OUTPUT_FILE_NAME + ext)
    else:
        header = create_header(width, height, splits, SPLIT_HEIGHT, lenbuf, ext)
        output_file_path = os.path.join(input_dir, OUTPUT_FILE_NAME + output_extension)

    save_image(output_file_path, header, split_data)

    print('Completed', input_filename, '->', os.path.basename(output_file_path))

def convert_image_to_qoi(input_file, height_str):
    process_image(input_file, height_str, '.sqoi', convert_to_qoi=True)

def convert_image_to_simg(input_file, height_str):
    input_dir, input_filename = os.path.split(input_file)
    _, ext = os.path.splitext(input_filename)
    output_extension = '.sjpg' if ext.lower() == '.jpg' else '.spng'
    process_image(input_file, height_str, output_extension, convert_to_qoi=False)

def convert_image_to_raw(input_file: str) -> None:
    """
    Convert an image to raw binary format compatible with LVGL.

    Parameters:
    - input_file (str): Path to the input image file.

    Raises:
    - FileNotFoundError: If required scripts are not found.
    - subprocess.CalledProcessError: If the external conversion script fails.
    - KeyError: If required keys are missing in config_data.
    """
    input_dir, input_filename = os.path.split(input_file)
    _, ext = os.path.splitext(input_filename)
    convert_path = os.path.join(os.path.dirname(input_file), 'lvgl_image_converter')
    lvgl_ver_str = config_data.get('lvgl_ver', '9.0.0')

    try:
        lvgl_version = version.parse(lvgl_ver_str)
    except version.InvalidVersion:
        print(f'Invalid LVGL version format: {lvgl_ver_str}')
        sys.exit(1)

    if lvgl_version >= version.parse('9.0.0'):
        handle_lvgl_version_v9(
            input_file=input_file,
            input_dir=input_dir,
            input_filename=input_filename,
            convert_path=convert_path
        )
    else:
        handle_lvgl_version_v8(
            input_file=input_file,
            input_dir=input_dir,
            input_filename=input_filename,
            convert_path=convert_path
        )

def pack_assets(config: PackModelsConfig):
    """
    Pack models based on the provided configuration.
    """

    target_path = config.target_path
    assets_include_path = config.include_path
    out_file = config.image_file
    assets_path = config.assets_path
    max_name_len = config.name_length

    merged_data = bytearray()
    file_info_list = []
    skip_files = ['config.json', 'lvgl_image_converter']

    file_list = sorted(os.listdir(target_path), key=sort_key)
    for filename in file_list:
        if filename in skip_files:
            continue

        file_path = os.path.join(target_path, filename)
        file_name = os.path.basename(file_path)
        file_size = os.path.getsize(file_path)

        try:
            img = Image.open(file_path)
            width, height = img.size
        except Exception as e:
            # print("Error:", e)
            _, file_extension = os.path.splitext(file_path)
            if file_extension.lower() in ['.sjpg', '.spng', '.sqoi']:
                offset = 14
                with open(file_path, 'rb') as f:
                    f.seek(offset)
                    width_bytes = f.read(2)
                    height_bytes = f.read(2)
                    width = int.from_bytes(width_bytes, byteorder='little')
                    height = int.from_bytes(height_bytes, byteorder='little')
            else:
                width, height = 0, 0

        file_info_list.append((file_name, len(merged_data), file_size, width, height))
        # Add 0x5A5A prefix to merged_data
        merged_data.extend(b'\x5A' * 2)

        with open(file_path, 'rb') as bin_file:
            bin_data = bin_file.read()

        merged_data.extend(bin_data)

    total_files = len(file_info_list)

    mmap_table = bytearray()
    for file_name, offset, file_size, width, height in file_info_list:
        if len(file_name) > int(max_name_len):
            print(f'\033[1;33mWarn:\033[0m "{file_name}" exceeds {max_name_len} bytes and will be truncated.')
        fixed_name = file_name.ljust(int(max_name_len), '\0')[:int(max_name_len)]
        mmap_table.extend(fixed_name.encode('utf-8'))
        mmap_table.extend(file_size.to_bytes(4, byteorder='little'))
        mmap_table.extend(offset.to_bytes(4, byteorder='little'))
        mmap_table.extend(width.to_bytes(2, byteorder='little'))
        mmap_table.extend(height.to_bytes(2, byteorder='little'))

    combined_data = mmap_table + merged_data
    combined_checksum = compute_checksum(combined_data)
    combined_data_length = len(combined_data).to_bytes(4, byteorder='little')
    header_data = total_files.to_bytes(4, byteorder='little') + combined_checksum.to_bytes(4, byteorder='little')
    final_data = header_data + combined_data_length + combined_data

    with open(out_file, 'wb') as output_bin:
        output_bin.write(final_data)

    os.makedirs(assets_include_path, exist_ok=True)
    current_year = datetime.now().year

    asset_name = os.path.basename(assets_path)
    file_path = os.path.join(assets_include_path, f'mmap_generate_{asset_name}.h')
    with open(file_path, 'w') as output_header:
        output_header.write('/*\n')
        output_header.write(' * SPDX-FileCopyrightText: 2022-{} Espressif Systems (Shanghai) CO LTD\n'.format(current_year))
        output_header.write(' *\n')
        output_header.write(' * SPDX-License-Identifier: Apache-2.0\n')
        output_header.write(' */\n\n')
        output_header.write('/**\n')
        output_header.write(' * @file\n')
        output_header.write(" * @brief This file was generated by esp_mmap_assets, don't modify it\n")
        output_header.write(' */\n\n')
        output_header.write('#pragma once\n\n')
        output_header.write("#include \"esp_mmap_assets.h\"\n\n")
        output_header.write(f'#define MMAP_{asset_name.upper()}_FILES           {total_files}\n')
        output_header.write(f'#define MMAP_{asset_name.upper()}_CHECKSUM        0x{combined_checksum:04X}\n\n')
        output_header.write(f'enum MMAP_{asset_name.upper()}_LISTS {{\n')

        for i, (file_name, _, _, _, _) in enumerate(file_info_list):
            enum_name = file_name.replace('.', '_')
            output_header.write(f'    MMAP_{asset_name.upper()}_{enum_name.upper()} = {i},        /*!< {file_name} */\n')

        output_header.write('};\n')

    print(f'All bin files have been merged into {os.path.basename(out_file)}')

def copy_assets(config: AssetCopyConfig):
    """
    Copy assets to target_path based on the provided configuration.
    """
    format_tuple = tuple(config.support_format)
    assets_path = config.assets_path
    target_path = config.target_path

    for filename in os.listdir(assets_path):
        if any(filename.endswith(suffix) for suffix in format_tuple):
            source_file = os.path.join(assets_path, filename)
            target_file = os.path.join(target_path, filename)
            shutil.copyfile(source_file, target_file)

            conversion_map = {
                '.jpg': [
                    (config.sjpg_enable, convert_image_to_simg),
                    (config.qoi_enable, convert_image_to_qoi),
                ],
                '.png': [
                    (config.spng_enable, convert_image_to_simg),
                    (config.qoi_enable, convert_image_to_qoi),
                ],
            }

            file_ext = os.path.splitext(filename)[1].lower()
            conversions = conversion_map.get(file_ext, [])
            converted = False

            for enable_flag, convert_func in conversions:
                if enable_flag:
                    convert_func(target_file, config.split_height)
                    os.remove(target_file)
                    converted = True
                    break

            if not converted and config.row_enable:
                convert_image_to_raw(target_file)
                os.remove(target_file)
        else:
            print(f'No match found for file: {filename}, format_tuple: {format_tuple}')

def process_assets_build(config_data):
    assets_path = config_data['assets_path']
    image_file = config_data['image_file']
    target_path = os.path.dirname(image_file)
    include_path = config_data['include_path']
    name_length = config_data['name_length']
    split_height = config_data['split_height']
    support_format = [fmt.strip() for fmt in config_data['support_format'].split(',')]

    copy_config = AssetCopyConfig(
        assets_path=assets_path,
        target_path=target_path,
        spng_enable=config_data['support_spng'],
        sjpg_enable=config_data['support_sjpg'],
        qoi_enable=config_data['support_qoi'],
        sqoi_enable=config_data['support_sqoi'],
        row_enable=config_data['support_raw'],
        support_format=support_format,
        split_height=split_height
    )

    pack_config = PackModelsConfig(
        target_path=target_path,
        include_path=include_path,
        image_file=image_file,
        assets_path=assets_path,
        name_length=name_length
    )

    print('--support_format:', support_format)

    if '.jpg' in support_format or '.png' in support_format:
        print('--support_spng:', copy_config.spng_enable)
        print('--support_sjpg:', copy_config.sjpg_enable)
        print('--support_qoi:', copy_config.qoi_enable)
        print('--support_raw:', copy_config.row_enable)

        if copy_config.sqoi_enable:
            print('--support_sqoi:', copy_config.sqoi_enable)
        if copy_config.spng_enable or copy_config.sjpg_enable or copy_config.sqoi_enable:
            print('--split_height:', copy_config.split_height)
        if copy_config.row_enable:
            print('--lvgl_version:', config_data['lvgl_ver'])

    if not os.path.exists(target_path):
        os.makedirs(target_path, exist_ok=True)
    for filename in os.listdir(target_path):
        file_path = os.path.join(target_path, filename)
        if os.path.isfile(file_path) or os.path.islink(file_path):
            os.unlink(file_path)
        elif os.path.isdir(file_path):
            shutil.rmtree(file_path)

    copy_assets(copy_config)
    pack_assets(pack_config)

    total_size = os.path.getsize(os.path.join(target_path, image_file))
    recommended_size = math.ceil(total_size / 1024)
    partition_size = math.ceil(int(config_data['assets_size'], 16))

    print(f'{"Total size:":<30} {GREEN}{total_size / 1024:>8.2f}K ({total_size}){RESET}')
    print(f'{"Partition size:":<30} {GREEN}{partition_size / 1024:>8.2f}K ({partition_size}){RESET}')

    if int(config_data['assets_size'], 16) <= total_size:
        print(f'Recommended partition size: {GREEN}{recommended_size}K{RESET}')
        print(f'{RED}Error:Binary size exceeds partition size.{RESET}')
        sys.exit(1)

def process_assets_merge(config_data):
    app_bin_path = config_data['app_bin_path']
    image_file = config_data['image_file']
    target_path = os.path.dirname(image_file)

    combined_bin_path = os.path.join(target_path, 'combined.bin')
    append_bin_path = os.path.join(target_path, image_file)

    app_size = os.path.getsize(app_bin_path)
    asset_size = os.path.getsize(append_bin_path)
    total_size = asset_size + app_size
    recommended_size = math.ceil(total_size / 1024)
    partition_size = math.ceil(int(config_data['assets_size'], 16))

    print(f'{"Asset size:":<30} {GREEN}{asset_size / 1024:>8.2f}K ({asset_size}){RESET}')
    print(f'{"App size:":<30} {GREEN}{app_size / 1024:>8.2f}K ({app_size}){RESET}')
    print(f'{"Total size:":<30} {GREEN}{total_size / 1024:>8.2f}K ({total_size}){RESET}')
    print(f'{"Partition size:":<30} {GREEN}{partition_size / 1024:>8.2f}K ({partition_size}){RESET}')

    if total_size > partition_size:
        print(f'Recommended partition size: {GREEN}{recommended_size}K{RESET}')
        print(f'{RED}Error:Binary size exceeds partition size.{RESET}')
        sys.exit(1)

    with open(combined_bin_path, 'wb') as combined_bin:
        with open(app_bin_path, 'rb') as app_bin:
            combined_bin.write(app_bin.read())
        with open(append_bin_path, 'rb') as img_bin:
            combined_bin.write(img_bin.read())

    shutil.move(combined_bin_path, app_bin_path)
    print(f'Append bin created: {os.path.basename(app_bin_path)}')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Move and Pack assets.')
    parser.add_argument('--config', required=True, help='Path to the configuration file')
    parser.add_argument('--merge', action='store_true', help='Merge assets with app binary')
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config_data = json.load(f)

    if args.merge:
        process_assets_merge(config_data)
    else:
        process_assets_build(config_data)
