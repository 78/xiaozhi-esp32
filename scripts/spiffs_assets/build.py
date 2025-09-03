#!/usr/bin/env python3
"""
Build the spiffs assets partition

Usage:
    ./build.py --wakenet_model <wakenet_model_dir> \
        --text_font <text_font_file> \
        --emoji_collection <emoji_collection_dir>

Example:
    ./build.py --wakenet_model ../../managed_components/espressif__esp-sr/model/wakenet_model/wn9_nihaoxiaozhi_tts \
        --text_font ../../components/xiaozhi-fonts/build/font_puhui_common_20_4.bin \
        --emoji_collection ../../components/xiaozhi-fonts/build/emojis_64/
"""

import os
import sys
import shutil
import argparse
import subprocess
import json
from pathlib import Path


def ensure_dir(directory):
    """Ensure directory exists, create if not"""
    os.makedirs(directory, exist_ok=True)


def copy_file(src, dst):
    """Copy file"""
    if os.path.exists(src):
        shutil.copy2(src, dst)
        print(f"Copied: {src} -> {dst}")
    else:
        print(f"Warning: Source file does not exist: {src}")


def copy_directory(src, dst):
    """Copy directory"""
    if os.path.exists(src):
        shutil.copytree(src, dst, dirs_exist_ok=True)
        print(f"Copied directory: {src} -> {dst}")
    else:
        print(f"Warning: Source directory does not exist: {src}")


def process_wakenet_model(wakenet_model_dir, build_dir, assets_dir):
    """Process wakenet_model parameter"""
    if not wakenet_model_dir:
        return None
    
    # Copy input directory to build directory
    wakenet_build_dir = os.path.join(build_dir, "wakenet_model")
    if os.path.exists(wakenet_build_dir):
        shutil.rmtree(wakenet_build_dir)
    copy_directory(wakenet_model_dir, os.path.join(wakenet_build_dir, os.path.basename(wakenet_model_dir)))
    
    # Use pack_model.py to generate srmodels.bin
    srmodels_output = os.path.join(wakenet_build_dir, "srmodels.bin")
    try:
        subprocess.run([
            sys.executable, "pack_model.py", 
            "-m", wakenet_build_dir, 
            "-o", "srmodels.bin"
        ], check=True, cwd=os.path.dirname(__file__))
        print(f"Generated: {srmodels_output}")
        # Copy srmodels.bin to assets directory
        copy_file(srmodels_output, os.path.join(assets_dir, "srmodels.bin"))
        return "srmodels.bin"
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to generate srmodels.bin: {e}")
        return None


def process_text_font(text_font_file, assets_dir):
    """Process text_font parameter"""
    if not text_font_file:
        return None
    
    # Copy input file to build/assets directory
    font_filename = os.path.basename(text_font_file)
    font_dst = os.path.join(assets_dir, font_filename)
    copy_file(text_font_file, font_dst)
    
    return font_filename


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
                copy_file(src_file, dst_file)
                
                # Get filename without extension
                filename_without_ext = os.path.splitext(file)[0]
                
                # Add to emoji list
                emoji_list.append({
                    "name": filename_without_ext,
                    "file": file
                })
    
    return emoji_list


def generate_index_json(assets_dir, srmodels, text_font, emoji_collection):
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
    
    # Write index.json
    index_path = os.path.join(assets_dir, "index.json")
    with open(index_path, 'w', encoding='utf-8') as f:
        json.dump(index_data, f, indent=4, ensure_ascii=False)
    
    print(f"Generated: {index_path}")


def generate_config_json(build_dir, assets_dir):
    """Generate config.json file"""
    # Get absolute path of current working directory
    workspace_dir = os.path.abspath(os.path.join(os.path.dirname(__file__)))
    
    config_data = {
        "include_path": os.path.join(workspace_dir, "build/include"),
        "assets_path": os.path.join(workspace_dir, "build/assets"),
        "image_file": os.path.join(workspace_dir, "build/output/assets.bin"),
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


def main():
    parser = argparse.ArgumentParser(description='Build the spiffs assets partition')
    parser.add_argument('--wakenet_model', help='Path to wakenet model directory')
    parser.add_argument('--text_font', help='Path to text font file')
    parser.add_argument('--emoji_collection', help='Path to emoji collection directory')
    
    args = parser.parse_args()
    
    # Get script directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Set directory paths
    build_dir = os.path.join(script_dir, "build")
    assets_dir = os.path.join(build_dir, "assets")
    if os.path.exists(assets_dir):
        shutil.rmtree(assets_dir)
    
    # Ensure directories exist
    ensure_dir(build_dir)
    ensure_dir(assets_dir)
    
    print("Starting to build SPIFFS assets partition...")
    
    # Process each parameter
    srmodels = process_wakenet_model(args.wakenet_model, build_dir, assets_dir)
    text_font = process_text_font(args.text_font, assets_dir)
    emoji_collection = process_emoji_collection(args.emoji_collection, assets_dir)
    
    # Generate index.json
    generate_index_json(assets_dir, srmodels, text_font, emoji_collection)
    
    # Generate config.json
    config_path = generate_config_json(build_dir, assets_dir)
    
    # Use spiffs_assets_gen.py to package final build/assets.bin
    try:
        subprocess.run([
            sys.executable, "spiffs_assets_gen.py", 
            "--config", config_path
        ], check=True, cwd=script_dir)
        print("Successfully packaged assets.bin")
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to package assets.bin: {e}")
        sys.exit(1)
    
    # Copy build/output/assets.bin to build/assets.bin
    shutil.copy(os.path.join(build_dir, "output", "assets.bin"), os.path.join(build_dir, "assets.bin"))
    print("Build completed!")


if __name__ == "__main__":
    main()