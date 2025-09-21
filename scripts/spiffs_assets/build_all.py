#!/usr/bin/env python3
"""
Build multiple spiffs assets partitions with different parameter combinations

This script calls build.py with different combinations of:
- wakenet_models
- text_fonts  
- emoji_collections

And generates assets.bin files with names like:
wn9_nihaoxiaozhi_tts-font_puhui_common_20_4-emojis_32.bin
"""

import os
import sys
import shutil
import subprocess
import argparse
from pathlib import Path


def ensure_dir(directory):
    """Ensure directory exists, create if not"""
    os.makedirs(directory, exist_ok=True)


def get_file_path(base_dir, filename):
    """Get full path for a file, handling 'none' case"""
    if filename == "none":
        return None
    return os.path.join(base_dir, f"{filename}.bin" if not filename.startswith("emojis_") else filename)


def build_assets(wakenet_model, text_font, emoji_collection, target_board, build_dir, final_dir):
    """Build assets.bin using build.py with given parameters"""
    
    # Prepare arguments for build.py
    cmd = [sys.executable, "build.py"]
    
    if wakenet_model != "none":
        wakenet_path = os.path.join("../../managed_components/espressif__esp-sr/model/wakenet_model", wakenet_model)
        cmd.extend(["--wakenet_model", wakenet_path])
    
    if text_font != "none":
        text_font_path = os.path.join("../../components/78__xiaozhi-fonts/cbin", f"{text_font}.bin")
        cmd.extend(["--text_font", text_font_path])
    
    if emoji_collection != "none":
        emoji_path = os.path.join("../../components/xiaozhi-fonts/build", emoji_collection)
        cmd.extend(["--emoji_collection", emoji_path])

    if target_board != "none":
        res_path = os.path.join("../../managed_components/espressif2022__esp_emote_gfx/emoji_large", "")
        cmd.extend(["--res_path", res_path])

        target_board_path = os.path.join("../../main/boards/", f"{target_board}")
        cmd.extend(["--target_board", target_board_path])
    
    print(f"\n正在构建: {wakenet_model}-{text_font}-{emoji_collection}-{target_board}")
    print(f"执行命令: {' '.join(cmd)}")
    
    try:
        # Run build.py
        result = subprocess.run(cmd, check=True, cwd=os.path.dirname(__file__))
        
        # Generate output filename
        if(target_board != "none"):
            output_name = f"{wakenet_model}-{text_font}-{target_board}.bin"
        else:
            output_name = f"{wakenet_model}-{text_font}-{emoji_collection}.bin"
        
        # Copy generated assets.bin to final directory with new name
        src_path = os.path.join(build_dir, "assets.bin")
        dst_path = os.path.join(final_dir, output_name)
        
        if os.path.exists(src_path):
            shutil.copy2(src_path, dst_path)
            print(f"✓ 成功生成: {output_name}")
            return True
        else:
            print(f"✗ 错误: 未找到生成的 assets.bin 文件")
            return False
            
    except subprocess.CalledProcessError as e:
        print(f"✗ 构建失败: {e}")
        return False
    except Exception as e:
        print(f"✗ 未知错误: {e}")
        return False


def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='构建多个 SPIFFS assets 分区')
    parser.add_argument('--mode',
                       choices=['emoji_collections', 'emoji_target_boards'],
                       default='emoji_collections',
                       help='选择运行模式: emoji_collections 或 emoji_target_boards (默认: emoji_collections)')

    args = parser.parse_args()
    
    # Configuration
    wakenet_models = [
        "none",
        "wn9_nihaoxiaozhi_tts",
        "wn9s_nihaoxiaozhi"
    ]
    
    text_fonts = [
        "none",
        "font_puhui_common_14_1",
        "font_puhui_common_16_4", 
        "font_puhui_common_20_4",
        "font_puhui_common_30_4",
    ]
    
    emoji_collections = [
        "none",
        "emojis_32",
        "emojis_64",
    ]

    emoji_target_boards = [
        "esp-box-3",
        "echoear",
    ]
    
    # Get script directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Set directory paths
    build_dir = os.path.join(script_dir, "build")
    final_dir = os.path.join(build_dir, "final")
    
    # Ensure directories exist
    ensure_dir(build_dir)
    ensure_dir(final_dir)
    
    print("开始构建多个 SPIFFS assets 分区...")
    print(f"运行模式: {args.mode}")
    print(f"输出目录: {final_dir}")
    
    # Track successful builds
    successful_builds = 0
    
    if args.mode == 'emoji_collections':
        # Calculate total combinations for emoji_collections mode
        total_combinations = len(wakenet_models) * len(text_fonts) * len(emoji_collections)
        
        # Build all combinations with emoji_collections
        for wakenet_model in wakenet_models:
            for text_font in text_fonts:
                for emoji_collection in emoji_collections:
                    if build_assets(wakenet_model, text_font, emoji_collection, "none", build_dir, final_dir):
                        successful_builds += 1
                        
    elif args.mode == 'emoji_target_boards':
        # Calculate total combinations for emoji_target_boards mode
        total_combinations = len(wakenet_models) * len(text_fonts) * len(emoji_target_boards)
        
        # Build all combinations with emoji_target_boards
        for wakenet_model in wakenet_models:
            for text_font in text_fonts:
                for emoji_target_board in emoji_target_boards:
                    if build_assets(wakenet_model, text_font, "none", emoji_target_board, build_dir, final_dir):
                        successful_builds += 1
    
    print(f"\n构建完成!")
    print(f"成功构建: {successful_builds}/{total_combinations}")
    print(f"输出文件位置: {final_dir}")
    
    # List generated files
    if os.path.exists(final_dir):
        files = [f for f in os.listdir(final_dir) if f.endswith('.bin')]
        if files:
            print("\n生成的文件:")
            for file in sorted(files):
                file_size = os.path.getsize(os.path.join(final_dir, file))
                print(f"  {file} ({file_size:,} bytes)")
        else:
            print("\n未找到生成的 .bin 文件")


if __name__ == "__main__":
    main()


