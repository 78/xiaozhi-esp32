#!/usr/bin/env python3
"""
Build default assets based on configuration

This script reads configuration from sdkconfig and builds the appropriate assets.bin
for the current board configuration.

Usage:
    ./build_default_assets.py --sdkconfig <path> --builtin_text_font <font_name> \
        --default_emoji_collection <collection_name> --output <output_path>
"""

import argparse
import io
import os
import shutil
import sys
import json
import struct
import math
from pathlib import Path
from datetime import datetime


# =============================================================================
# Pack model functions (from pack_model.py)
# =============================================================================

def struct_pack_string(string, max_len=None):
    """
    pack string to binary data. 
    if max_len is None, max_len = len(string) + 1
    else len(string) < max_len, the left will be padded by struct.pack('x')
    """
    if max_len == None :
        max_len = len(string)
    else:
        assert len(string) <= max_len

    left_num = max_len - len(string)
    out_bytes = None
    for char in string:
        if out_bytes == None:
            out_bytes = struct.pack('b', ord(char))
        else:
            out_bytes += struct.pack('b', ord(char))
    for i in range(left_num):
        out_bytes += struct.pack('x')
    return out_bytes


def read_data(filename):
    """Read binary data, like index and mndata"""
    data = None
    with open(filename, "rb") as f:
        data = f.read()
    return data


def pack_models(model_path, out_file="srmodels.bin"):
    """
    Pack all models into one binary file by the following format:
    {
        model_num: int
        model1_info: model_info_t
        model2_info: model_info_t
        ...
        model1_index,model1_data,model1_MODEL_INFO
        model1_index,model1_data,model1_MODEL_INFO
        ...
    }model_pack_t

    {
        model_name: char[32]
        file_number: int
        file1_name: char[32]
        file1_start: int  
        file1_len: int 
        file2_name: char[32]
        file2_start: int   // data_len = info_start - data_start
        file2_len: int
        ...
    }model_info_t
    """
    models = {}
    file_num = 0
    model_num = 0
    for root, dirs, _ in os.walk(model_path):
        for model_name in dirs:
            models[model_name] = {}
            model_dir = os.path.join(root, model_name)
            model_num += 1
            for _, _, files in os.walk(model_dir):
                for file_name in files:
                    file_num += 1
                    file_path = os.path.join(model_dir, file_name)
                    models[model_name][file_name] = read_data(file_path)
    
    model_num = len(models)
    header_len = 4 + model_num*(32+4) + file_num*(32+4+4) 
    out_bin = struct.pack('I', model_num)  # model number
    data_bin = None
    for key in models:
        model_bin = struct_pack_string(key, 32) # + model name
        model_bin += struct.pack('I', len(models[key])) # + file number in this model
        
        for file_name in models[key]:
            model_bin += struct_pack_string(file_name, 32) # + file name
            if data_bin == None:
                model_bin += struct.pack('I', header_len) 
                data_bin = models[key][file_name]
                model_bin += struct.pack('I', len(models[key][file_name]))
            else:
                model_bin += struct.pack('I', header_len+len(data_bin))
                data_bin += models[key][file_name]
                model_bin += struct.pack('I', len(models[key][file_name]))
        
        out_bin += model_bin
    assert len(out_bin) == header_len
    if data_bin != None:
        out_bin += data_bin

    out_file = os.path.join(model_path, out_file)
    with open(out_file, "wb") as f:
        f.write(out_bin)


# =============================================================================
# Build assets functions (from build.py)
# =============================================================================

def ensure_dir(directory):
    """Ensure directory exists, create if not"""
    os.makedirs(directory, exist_ok=True)


def copy_file(src, dst):
    """Copy file"""
    if os.path.exists(src):
        shutil.copy2(src, dst)
        print(f"Copied: {src} -> {dst}")
        return True
    else:
        print(f"Warning: Source file does not exist: {src}")
        return False


def copy_directory(src, dst):
    """Copy directory"""
    if os.path.exists(src):
        shutil.copytree(src, dst, dirs_exist_ok=True)
        print(f"Copied directory: {src} -> {dst}")
        return True
    else:
        print(f"Warning: Source directory does not exist: {src}")
        return False


def process_sr_models(wakenet_model_dir, multinet_model_dirs, build_dir, assets_dir):
    """Process SR models (wakenet and multinet) and generate srmodels.bin"""
    if not wakenet_model_dir and not multinet_model_dirs:
        return None
    
    # Create SR models build directory
    sr_models_build_dir = os.path.join(build_dir, "srmodels")
    if os.path.exists(sr_models_build_dir):
        shutil.rmtree(sr_models_build_dir)
    os.makedirs(sr_models_build_dir)
    
    models_processed = 0
    
    # Copy wakenet model if available
    if wakenet_model_dir:
        wakenet_name = os.path.basename(wakenet_model_dir)
        wakenet_dst = os.path.join(sr_models_build_dir, wakenet_name)
        if copy_directory(wakenet_model_dir, wakenet_dst):
            models_processed += 1
            print(f"Added wakenet model: {wakenet_name}")
    
    # Copy multinet models if available
    if multinet_model_dirs:
        for multinet_model_dir in multinet_model_dirs:
            multinet_name = os.path.basename(multinet_model_dir)
            multinet_dst = os.path.join(sr_models_build_dir, multinet_name)
            if copy_directory(multinet_model_dir, multinet_dst):
                models_processed += 1
                print(f"Added multinet model: {multinet_name}")
    
    if models_processed == 0:
        print("Warning: No SR models were successfully processed")
        return None
    
    # Use pack_models function to generate srmodels.bin
    srmodels_output = os.path.join(sr_models_build_dir, "srmodels.bin")
    try:
        pack_models(sr_models_build_dir, "srmodels.bin")
        print(f"Generated: {srmodels_output}")
        # Copy srmodels.bin to assets directory
        copy_file(srmodels_output, os.path.join(assets_dir, "srmodels.bin"))
        return "srmodels.bin"
    except Exception as e:
        print(f"Error: Failed to generate srmodels.bin: {e}")
        return None


def process_wakenet_model(wakenet_model_dir, build_dir, assets_dir):
    """Process wakenet_model parameter (legacy compatibility function)"""
    return process_sr_models(wakenet_model_dir, None, build_dir, assets_dir)


def process_text_font(text_font_file, assets_dir):
    """Process text_font parameter"""
    if not text_font_file:
        return None
    
    # Copy input file to build/assets directory
    font_filename = os.path.basename(text_font_file)
    font_dst = os.path.join(assets_dir, font_filename)
    if copy_file(text_font_file, font_dst):
        return font_filename
    return None


def process_emoji_collection(emoji_collection_dir, assets_dir):
    """Process emoji_collection parameter"""
    if not emoji_collection_dir:
        return []
    
    emoji_list = []
    
    # Copy each image from input directory to build/assets directory
    for root, dirs, files in os.walk(emoji_collection_dir):
        for file in files:
            if file.lower().endswith(('.png', '.gif')):
                # Copy file
                src_file = os.path.join(root, file)
                dst_file = os.path.join(assets_dir, file)
                if copy_file(src_file, dst_file):
                    # Get filename without extension
                    filename_without_ext = os.path.splitext(file)[0]
                    
                    # Add to emoji list
                    emoji_list.append({
                        "name": filename_without_ext,
                        "file": file
                    })
    
    return emoji_list


def process_extra_files(extra_files_dir, assets_dir):
    """Process default_assets_extra_files parameter"""
    if not extra_files_dir:
        return []
    
    if not os.path.exists(extra_files_dir):
        print(f"Warning: Extra files directory not found: {extra_files_dir}")
        return []
    
    extra_files_list = []
    
    # Copy each file from input directory to build/assets directory
    for root, dirs, files in os.walk(extra_files_dir):
        for file in files:
            # Skip hidden files and directories
            if file.startswith('.'):
                continue
                
            # Copy file
            src_file = os.path.join(root, file)
            dst_file = os.path.join(assets_dir, file)
            if copy_file(src_file, dst_file):
                extra_files_list.append(file)
    
    if extra_files_list:
        print(f"Processed {len(extra_files_list)} extra files from: {extra_files_dir}")
    
    return extra_files_list


def generate_index_json(assets_dir, srmodels, text_font, emoji_collection, extra_files=None, multinet_model_info=None):
    """Generate index.json file"""
    index_data = {
        "version": 1
    }
    
    if srmodels:
        index_data["srmodels"] = srmodels
    
    if text_font:
        index_data["text_font"] = text_font
    
    if emoji_collection:
        index_data["emoji_collection"] = emoji_collection
    
    if extra_files:
        index_data["extra_files"] = extra_files
    
    if multinet_model_info:
        index_data["multinet_model"] = multinet_model_info
    
    # Write index.json
    index_path = os.path.join(assets_dir, "index.json")
    with open(index_path, 'w', encoding='utf-8') as f:
        json.dump(index_data, f, indent=4, ensure_ascii=False)
    
    print(f"Generated: {index_path}")


def generate_config_json(build_dir, assets_dir):
    """Generate config.json file"""
    config_data = {
        "include_path": os.path.join(build_dir, "include"),
        "assets_path": assets_dir,
        "image_file": os.path.join(build_dir, "output", "assets.bin"),
        "lvgl_ver": "9.3.0",
        "assets_size": "0x400000",
        "support_format": ".png, .gif, .jpg, .bin, .json",
        "name_length": "32",
        "split_height": "0",
        "support_qoi": False,
        "support_spng": False,
        "support_sjpg": False,
        "support_sqoi": False,
        "support_raw": False,
        "support_raw_dither": False,
        "support_raw_bgr": False
    }
    
    # Write config.json
    config_path = os.path.join(build_dir, "config.json")
    with open(config_path, 'w', encoding='utf-8') as f:
        json.dump(config_data, f, indent=4, ensure_ascii=False)
    
    print(f"Generated: {config_path}")
    return config_path


# =============================================================================
# Simplified SPIFFS assets generation (from spiffs_assets_gen.py)
# =============================================================================

def compute_checksum(data):
    checksum = sum(data) & 0xFFFF
    return checksum


def sort_key(filename):
    basename, extension = os.path.splitext(filename)
    return extension, basename


def pack_assets_simple(target_path, include_path, out_file, assets_path, max_name_len=32):
    """
    Simplified version of pack_assets that handles basic file packing
    """
    merged_data = bytearray()
    file_info_list = []
    skip_files = ['config.json']

    # Ensure output directory exists
    os.makedirs(os.path.dirname(out_file), exist_ok=True)
    os.makedirs(include_path, exist_ok=True)

    file_list = sorted(os.listdir(target_path), key=sort_key)
    for filename in file_list:
        if filename in skip_files:
            continue

        file_path = os.path.join(target_path, filename)
        if not os.path.isfile(file_path):
            continue
            
        file_name = os.path.basename(file_path)
        file_size = os.path.getsize(file_path)

        file_info_list.append((file_name, len(merged_data), file_size, 0, 0))
        # Add 0x5A5A prefix to merged_data
        merged_data.extend(b'\x5A' * 2)

        with open(file_path, 'rb') as bin_file:
            bin_data = bin_file.read()

        merged_data.extend(bin_data)

    total_files = len(file_info_list)

    mmap_table = bytearray()
    for file_name, offset, file_size, width, height in file_info_list:
        if len(file_name) > max_name_len:
            print(f'Warning: "{file_name}" exceeds {max_name_len} bytes and will be truncated.')
        fixed_name = file_name.ljust(max_name_len, '\0')[:max_name_len]
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

    # Generate header file
    current_year = datetime.now().year
    asset_name = os.path.basename(assets_path)
    header_file_path = os.path.join(include_path, f'mmap_generate_{asset_name}.h')
    with open(header_file_path, 'w') as output_header:
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

    print(f'All files have been merged into {os.path.basename(out_file)}')


# =============================================================================
# Configuration and main functions
# =============================================================================

def read_wakenet_from_sdkconfig(sdkconfig_path):
    """
    Read wakenet model from sdkconfig (based on movemodel.py logic)
    Returns the wakenet model name or None if no wakenet is configured
    """
    if not os.path.exists(sdkconfig_path):
        print(f"Warning: sdkconfig file not found: {sdkconfig_path}")
        return None
        
    models = []
    with io.open(sdkconfig_path, "r") as f:
        for label in f:
            label = label.strip("\n")
            if 'CONFIG_SR_WN' in label and '#' not in label[0]:
                if '_NONE' in label:
                    continue
                if '=' in label:
                    label = label.split("=")[0]
                if '_MULTI' in label:
                    label = label[:-6]
                model_name = label.split("_SR_WN_")[-1].lower()
                models.append(model_name)

    # Return the first model found, or None if no models
    return models[0] if models else None


def read_multinet_from_sdkconfig(sdkconfig_path):
    """
    Read multinet models from sdkconfig (based on movemodel.py logic)
    Returns a list of multinet model names
    """
    if not os.path.exists(sdkconfig_path):
        print(f"Warning: sdkconfig file not found: {sdkconfig_path}")
        return []
        
    with io.open(sdkconfig_path, "r") as f:
        models_string = ''
        for label in f:
            label = label.strip("\n")
            if 'CONFIG_SR_MN' in label and label[0] != '#':
                models_string += label

    models = []
    if "CONFIG_SR_MN_CN_MULTINET3_SINGLE_RECOGNITION" in models_string:
        models.append('mn3_cn')
    elif "CONFIG_SR_MN_CN_MULTINET4_5_SINGLE_RECOGNITION_QUANT8" in models_string:
        models.append('mn4q8_cn')
    elif "CONFIG_SR_MN_CN_MULTINET4_5_SINGLE_RECOGNITION" in models_string:
        models.append('mn4_cn')
    elif "CONFIG_SR_MN_CN_MULTINET5_RECOGNITION_QUANT8" in models_string:
        models.append('mn5q8_cn')
    elif "CONFIG_SR_MN_CN_MULTINET6_QUANT" in models_string:
        models.append('mn6_cn')
    elif "CONFIG_SR_MN_CN_MULTINET6_AC_QUANT" in models_string:
        models.append('mn6_cn_ac')
    elif "CONFIG_SR_MN_CN_MULTINET7_QUANT" in models_string:
        models.append('mn7_cn')
    elif "CONFIG_SR_MN_CN_MULTINET7_AC_QUANT" in models_string:
        models.append('mn7_cn_ac')

    if "CONFIG_SR_MN_EN_MULTINET5_SINGLE_RECOGNITION_QUANT8" in models_string:
        models.append('mn5q8_en')
    elif "CONFIG_SR_MN_EN_MULTINET5_SINGLE_RECOGNITION" in models_string:
        models.append('mn5_en')
    elif "CONFIG_SR_MN_EN_MULTINET6_QUANT" in models_string:
        models.append('mn6_en')
    elif "CONFIG_SR_MN_EN_MULTINET7_QUANT" in models_string:
        models.append('mn7_en')

    if "MULTINET6" in models_string or "MULTINET7" in models_string:
        models.append('fst')

    return models


def read_custom_wake_word_from_sdkconfig(sdkconfig_path):
    """
    Read custom wake word configuration from sdkconfig
    Returns a dict with custom wake word info or None if not configured
    """
    if not os.path.exists(sdkconfig_path):
        print(f"Warning: sdkconfig file not found: {sdkconfig_path}")
        return None
        
    config_values = {}
    with io.open(sdkconfig_path, "r") as f:
        for line in f:
            line = line.strip("\n")
            if line.startswith('#') or '=' not in line:
                continue
                
            # Check for custom wake word configuration
            if 'CONFIG_USE_CUSTOM_WAKE_WORD=y' in line:
                config_values['use_custom_wake_word'] = True
            elif 'CONFIG_CUSTOM_WAKE_WORD=' in line and not line.startswith('#'):
                # Extract string value (remove quotes)
                value = line.split('=', 1)[1].strip('"')
                config_values['wake_word'] = value
            elif 'CONFIG_CUSTOM_WAKE_WORD_DISPLAY=' in line and not line.startswith('#'):
                # Extract string value (remove quotes)
                value = line.split('=', 1)[1].strip('"')
                config_values['display'] = value
            elif 'CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=' in line and not line.startswith('#'):
                # Extract numeric value
                value = line.split('=', 1)[1]
                try:
                    config_values['threshold'] = int(value)
                except ValueError:
                    try:
                        config_values['threshold'] = float(value)
                    except ValueError:
                        print(f"Warning: Invalid threshold value: {value}")
                        config_values['threshold'] = 20  # default (will be converted to 0.2)
    
    # Return config only if custom wake word is enabled and required fields are present
    if (config_values.get('use_custom_wake_word', False) and 
        'wake_word' in config_values and 
        'display' in config_values and 
        'threshold' in config_values):
        return {
            'wake_word': config_values['wake_word'],
            'display': config_values['display'],
            'threshold': config_values['threshold'] / 100.0  # Convert to decimal (20 -> 0.2)
        }
    
    return None


def get_language_from_multinet_models(multinet_models):
    """
    Determine language from multinet model names
    Returns 'cn', 'en', or None
    """
    if not multinet_models:
        return None
    
    # Check for Chinese models
    cn_indicators = ['_cn', 'cn_']
    en_indicators = ['_en', 'en_']
    
    has_cn = any(any(indicator in model for indicator in cn_indicators) for model in multinet_models)
    has_en = any(any(indicator in model for indicator in en_indicators) for model in multinet_models)
    
    # If both or neither, default to cn
    if has_cn and not has_en:
        return 'cn'
    elif has_en and not has_cn:
        return 'en'
    else:
        return 'cn'  # Default to Chinese


def get_wakenet_model_path(model_name, esp_sr_model_path):
    """
    Get the full path to the wakenet model directory
    """
    if not model_name:
        return None
    
    wakenet_model_path = os.path.join(esp_sr_model_path, 'wakenet_model', model_name)
    if os.path.exists(wakenet_model_path):
        return wakenet_model_path
    else:
        print(f"Warning: Wakenet model directory not found: {wakenet_model_path}")
        return None


def get_multinet_model_paths(model_names, esp_sr_model_path):
    """
    Get the full paths to the multinet model directories
    Returns a list of valid model paths
    """
    if not model_names:
        return []
    
    valid_paths = []
    for model_name in model_names:
        multinet_model_path = os.path.join(esp_sr_model_path, 'multinet_model', model_name)
        if os.path.exists(multinet_model_path):
            valid_paths.append(multinet_model_path)
        else:
            print(f"Warning: Multinet model directory not found: {multinet_model_path}")
    
    return valid_paths


def get_text_font_path(builtin_text_font, xiaozhi_fonts_path):
    """
    Get the text font path if needed
    Returns the font file path or None if no font is needed
    """
    if not builtin_text_font or 'basic' not in builtin_text_font:
        return None
    
    # Convert from basic to common font name
    # e.g., font_puhui_basic_16_4 -> font_puhui_common_16_4.bin
    font_name = builtin_text_font.replace('basic', 'common') + '.bin'
    font_path = os.path.join(xiaozhi_fonts_path, 'cbin', font_name)
    
    if os.path.exists(font_path):
        return font_path
    else:
        print(f"Warning: Font file not found: {font_path}")
        return None


def get_emoji_collection_path(default_emoji_collection, xiaozhi_fonts_path):
    """
    Get the emoji collection path if needed
    Returns the emoji directory path or None if no emoji collection is needed
    """
    if not default_emoji_collection:
        return None
    
    emoji_path = os.path.join(xiaozhi_fonts_path, 'png', default_emoji_collection)
    if os.path.exists(emoji_path):
        return emoji_path
    else:
        print(f"Warning: Emoji collection directory not found: {emoji_path}")
        return None


def build_assets_integrated(wakenet_model_path, multinet_model_paths, text_font_path, emoji_collection_path, extra_files_path, output_path, multinet_model_info=None):
    """
    Build assets using integrated functions (no external dependencies)
    """
    # Create temporary build directory
    temp_build_dir = os.path.join(os.path.dirname(output_path), "temp_build")
    assets_dir = os.path.join(temp_build_dir, "assets")
    
    try:
        # Clean and create directories
        if os.path.exists(temp_build_dir):
            shutil.rmtree(temp_build_dir)
        ensure_dir(temp_build_dir)
        ensure_dir(assets_dir)
        
        print("Starting to build assets...")
        
        # Process each component
        srmodels = process_sr_models(wakenet_model_path, multinet_model_paths, temp_build_dir, assets_dir) if (wakenet_model_path or multinet_model_paths) else None
        text_font = process_text_font(text_font_path, assets_dir) if text_font_path else None
        emoji_collection = process_emoji_collection(emoji_collection_path, assets_dir) if emoji_collection_path else None
        extra_files = process_extra_files(extra_files_path, assets_dir) if extra_files_path else None
        
        # Generate index.json
        generate_index_json(assets_dir, srmodels, text_font, emoji_collection, extra_files, multinet_model_info)
        
        # Generate config.json for packing
        config_path = generate_config_json(temp_build_dir, assets_dir)
        
        # Load config and pack assets
        with open(config_path, 'r') as f:
            config_data = json.load(f)
        
        # Use simplified packing function
        include_path = config_data['include_path']
        image_file = config_data['image_file']
        pack_assets_simple(assets_dir, include_path, image_file, "assets", int(config_data['name_length']))
        
        # Copy final assets.bin to output location
        if os.path.exists(image_file):
            shutil.copy2(image_file, output_path)
            print(f"Successfully generated assets.bin: {output_path}")
            
            # Show size information
            total_size = os.path.getsize(output_path)
            print(f"Assets file size: {total_size / 1024:.2f}K ({total_size} bytes)")
            
            return True
        else:
            print(f"Error: Generated assets.bin not found: {image_file}")
            return False
            
    except Exception as e:
        print(f"Error: Failed to build assets: {e}")
        return False
    finally:
        # Clean up temporary directory
        if os.path.exists(temp_build_dir):
            shutil.rmtree(temp_build_dir)


def main():
    parser = argparse.ArgumentParser(description='Build default assets based on configuration')
    parser.add_argument('--sdkconfig', required=True, help='Path to sdkconfig file')
    parser.add_argument('--builtin_text_font', help='Builtin text font name (e.g., font_puhui_basic_16_4)')
    parser.add_argument('--emoji_collection', help='Default emoji collection name (e.g., emojis_32)')
    parser.add_argument('--output', required=True, help='Output path for assets.bin')
    parser.add_argument('--esp_sr_model_path', help='Path to ESP-SR model directory')
    parser.add_argument('--xiaozhi_fonts_path', help='Path to xiaozhi-fonts component directory')
    parser.add_argument('--extra_files', help='Path to extra files directory to be included in assets')
    
    args = parser.parse_args()
    
    # Get script directory (not needed anymore but keep for future use)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Set default paths if not provided
    if not args.esp_sr_model_path:
        # Default ESP-SR model path relative to project root
        project_root = os.path.dirname(os.path.dirname(script_dir))
        args.esp_sr_model_path = os.path.join(project_root, "managed_components", "espressif__esp-sr", "model")
    
    if not args.xiaozhi_fonts_path:
        # Default xiaozhi-fonts path relative to project root
        project_root = os.path.dirname(os.path.dirname(script_dir))
        args.xiaozhi_fonts_path = os.path.join(project_root, "managed_components", "78__xiaozhi-fonts")
    
    print("Building default assets...")
    print(f"  sdkconfig: {args.sdkconfig}")
    print(f"  builtin_text_font: {args.builtin_text_font}")
    print(f"  emoji_collection: {args.emoji_collection}")
    print(f"  output: {args.output}")
    
    # Read SR models from sdkconfig
    wakenet_model_name = read_wakenet_from_sdkconfig(args.sdkconfig)
    multinet_model_names = read_multinet_from_sdkconfig(args.sdkconfig)
    
    # Get model paths
    wakenet_model_path = get_wakenet_model_path(wakenet_model_name, args.esp_sr_model_path)
    multinet_model_paths = get_multinet_model_paths(multinet_model_names, args.esp_sr_model_path)
    
    # Print model information
    if wakenet_model_name:
        print(f"  wakenet model: {wakenet_model_name}")
    if multinet_model_names:
        print(f"  multinet models: {', '.join(multinet_model_names)}")
    
    # Get text font path if needed
    text_font_path = get_text_font_path(args.builtin_text_font, args.xiaozhi_fonts_path)
    
    # Get emoji collection path if needed
    emoji_collection_path = get_emoji_collection_path(args.emoji_collection, args.xiaozhi_fonts_path)
    
    # Get extra files path if provided
    extra_files_path = args.extra_files
    
    # Read custom wake word configuration
    custom_wake_word_config = read_custom_wake_word_from_sdkconfig(args.sdkconfig)
    multinet_model_info = None
    
    if custom_wake_word_config and multinet_model_names:
        # Determine language from multinet models
        language = get_language_from_multinet_models(multinet_model_names)
        
        # Build multinet_model info structure
        multinet_model_info = {
            "language": language,
            "duration": 3000,  # Default duration in ms
            "threshold": custom_wake_word_config['threshold'],
            "commands": [
                {
                    "command": custom_wake_word_config['wake_word'],
                    "text": custom_wake_word_config['display'],
                    "action": "wake"
                }
            ]
        }
        print(f"  custom wake word: {custom_wake_word_config['wake_word']} ({custom_wake_word_config['display']})")
        print(f"  wake word language: {language}")
        print(f"  wake word threshold: {custom_wake_word_config['threshold']}")
    
    # Check if we have anything to build
    if not wakenet_model_path and not multinet_model_paths and not text_font_path and not emoji_collection_path and not extra_files_path and not multinet_model_info:
        print("Warning: No assets to build (no SR models, text font, emoji collection, extra files, or custom wake word)")
        # Create an empty assets.bin file
        os.makedirs(os.path.dirname(args.output), exist_ok=True)
        with open(args.output, 'wb') as f:
            pass  # Create empty file
        print(f"Created empty assets.bin: {args.output}")
        return
    
    # Build the assets
    success = build_assets_integrated(wakenet_model_path, multinet_model_paths, text_font_path, emoji_collection_path, 
                                     extra_files_path, args.output, multinet_model_info)
    
    if not success:
        sys.exit(1)
    
    print("Build completed successfully!")


if __name__ == "__main__":
    main()
