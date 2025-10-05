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

def load_emoji_config(emoji_collection_dir):
    """Load emoji config from config.json file"""
    config_path = os.path.join(emoji_collection_dir, "emote.json")
    if not os.path.exists(config_path):
        print(f"Warning: Config file not found: {config_path}")
        return {}
    
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            config_data = json.load(f)
        
        # Convert list format to dict for easy lookup
        config_dict = {}
        for item in config_data:
            if "emote" in item:
                config_dict[item["emote"]] = item
        
        return config_dict
    except Exception as e:
        print(f"Error loading config file {config_path}: {e}")
        return {}

def process_board_emoji_collection(emoji_collection_dir, target_board_dir, assets_dir):
    """Process emoji_collection parameter"""
    if not emoji_collection_dir:
        return []
    
    emoji_config = load_emoji_config(target_board_dir)
    print(f"Loaded emoji config with {len(emoji_config)} entries")
    
    emoji_list = []
    
    for emote_name, config in emoji_config.items():

        if "src" not in config:
            print(f"Error: No src field found for emote '{emote_name}' in config")
            continue
        
        eaf_file_path = os.path.join(emoji_collection_dir, config["src"])
        file_exists = os.path.exists(eaf_file_path)
        
        if not file_exists:
            print(f"Warning: EAF file not found for emote '{emote_name}': {eaf_file_path}")
        else:
            # Copy eaf file to assets directory
            copy_file(eaf_file_path, os.path.join(assets_dir, config["src"]))
        
        # Create emoji entry with src as file (merge file and src)
        emoji_entry = {
            "name": emote_name,
            "file": config["src"]  # Use src as the actual file
        }
        
        eaf_properties = {}
        
        if not file_exists:
            eaf_properties["lack"] = True
        
        if "loop" in config:
            eaf_properties["loop"] = config["loop"]
        
        if "fps" in config:
            eaf_properties["fps"] = config["fps"]
        
        if eaf_properties:
            emoji_entry["eaf"] = eaf_properties
        
        status = "MISSING" if not file_exists else "OK"
        eaf_info = emoji_entry.get('eaf', {})
        print(f"emote '{emote_name}': file='{emoji_entry['file']}', status={status}, lack={eaf_info.get('lack', False)}, loop={eaf_info.get('loop', 'none')}, fps={eaf_info.get('fps', 'none')}")
        
        emoji_list.append(emoji_entry)
    
    print(f"Successfully processed {len(emoji_list)} emotes from config")
    return emoji_list

def process_board_icon_collection(icon_collection_dir, assets_dir):
    """Process emoji_collection parameter"""
    if not icon_collection_dir:
        return []
    
    icon_list = []
    
    for root, dirs, files in os.walk(icon_collection_dir):
        for file in files:
            if file.lower().endswith(('.bin')) or file.lower() == 'listen.eaf':
                src_file = os.path.join(root, file)
                dst_file = os.path.join(assets_dir, file)
                copy_file(src_file, dst_file)
                
                filename_without_ext = os.path.splitext(file)[0]

                icon_list.append({
                    "name": filename_without_ext,
                    "file": file
                })
    
    return icon_list
def process_board_layout(layout_json_file, assets_dir):
    """Process layout_json parameter"""
    if not layout_json_file:
        print(f"Warning: Layout json file not provided")
        return []
    
    print(f"Processing layout_json: {layout_json_file}")
    print(f"assets_dir: {assets_dir}")
    
    if os.path.isdir(layout_json_file):
        layout_json_path = os.path.join(layout_json_file, "layout.json")
        if not os.path.exists(layout_json_path):
            print(f"Warning: layout.json not found in directory: {layout_json_file}")
            return []
        layout_json_file = layout_json_path
    elif not os.path.isfile(layout_json_file):
        print(f"Warning: Layout json file not found: {layout_json_file}")
        return []
        
    try:
        with open(layout_json_file, 'r', encoding='utf-8') as f:
            layout_data = json.load(f)
        
        # Layout data is now directly an array, no need to get "layout" key
        layout_items = layout_data if isinstance(layout_data, list) else layout_data.get("layout", [])
        
        processed_layout = []
        for item in layout_items:
            processed_item = {
                "name": item.get("name", ""),
                "align": item.get("align", ""),
                "x": item.get("x", 0),
                "y": item.get("y", 0)
            }
            
            if "width" in item:
                processed_item["width"] = item["width"]
            if "height" in item:
                processed_item["height"] = item["height"]
                
            processed_layout.append(processed_item)
        
        print(f"Processed {len(processed_layout)} layout elements")
        return processed_layout
        
    except Exception as e:
        print(f"Error reading/processing layout.json: {e}")
        return []

def process_board_collection(target_board_dir, res_path, assets_dir):
    """Process board collection - merge icon, emoji, and layout processing"""
    
    # Process all collections
    if os.path.exists(res_path) and os.path.exists(target_board_dir):
        emoji_collection = process_board_emoji_collection(res_path, target_board_dir, assets_dir)
        icon_collection = process_board_icon_collection(res_path, assets_dir)
        layout_json = process_board_layout(target_board_dir, assets_dir)
    else:
        print(f"Warning: EAF directory not found: {res_path} or {target_board_dir}")
        emoji_collection = []
        icon_collection = []
        layout_json = []
    
    return emoji_collection, icon_collection, layout_json

def generate_index_json(assets_dir, srmodels, text_font, emoji_collection, icon_collection, layout_json):
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

    if icon_collection:
        index_data["icon_collection"] = icon_collection
    
    if layout_json:
        index_data["layout"] = layout_json
    
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
        "support_format": ".png, .gif, .jpg, .bin, .json, .eaf",
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

    parser.add_argument('--res_path', help='Path to res directory')
    parser.add_argument('--target_board', help='Path to target board directory')
    
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

    if(args.target_board):
        emoji_collection, icon_collection, layout_json = process_board_collection(args.target_board, args.res_path, assets_dir)
    else:
        emoji_collection = process_emoji_collection(args.emoji_collection, assets_dir)
        icon_collection = []
        layout_json = []
    
    # Generate index.json
    generate_index_json(assets_dir, srmodels, text_font, emoji_collection, icon_collection, layout_json)
    
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