#!/usr/bin/env python3
import argparse
import json
import os

HEADER_TEMPLATE = """// Auto-generated language config
#pragma once

#include <string_view>

#ifndef {lang_code_for_font}
    #define {lang_code_for_font}  // 預設語言
#endif

namespace Lang {{
    // 语言元数据
    constexpr const char* CODE = "{lang_code}";

    // 字符串资源
    namespace Strings {{
{strings}
    }}

    // 音效资源
    namespace Sounds {{
{sounds}
    }}
}}
"""

def generate_header(input_path, output_path):
    with open(input_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # 验证数据结构
    if 'language' not in data or 'strings' not in data:
        raise ValueError("Invalid JSON structure")

    lang_code = data['language']['type']

    # 生成字符串常量
    strings = []
    sounds = []
    for key, value in data['strings'].items():
        value = value.replace('"', '\\"')
        strings.append(f'        constexpr const char* {key.upper()} = "{value}";')

    # 生成音效常量
    for file in os.listdir(os.path.dirname(input_path)):
        if file.endswith('.p3'):
            base_name = os.path.splitext(file)[0]
            sounds.append(f'''
        extern const char p3_{base_name}_start[] asm("_binary_{base_name}_p3_start");
        extern const char p3_{base_name}_end[] asm("_binary_{base_name}_p3_end");
        static const std::string_view P3_{base_name.upper()} {{
        static_cast<const char*>(p3_{base_name}_start),
        static_cast<size_t>(p3_{base_name}_end - p3_{base_name}_start)
        }};''')
    
    # 生成公共音效
    for file in os.listdir(os.path.join(os.path.dirname(output_path), 'common')):
        if file.endswith('.p3'):
            base_name = os.path.splitext(file)[0]
            sounds.append(f'''
        extern const char p3_{base_name}_start[] asm("_binary_{base_name}_p3_start");
        extern const char p3_{base_name}_end[] asm("_binary_{base_name}_p3_end");
        static const std::string_view P3_{base_name.upper()} {{
        static_cast<const char*>(p3_{base_name}_start),
        static_cast<size_t>(p3_{base_name}_end - p3_{base_name}_start)
        }};''')

    # 填充模板
    content = HEADER_TEMPLATE.format(
        lang_code=lang_code,
        lang_code_for_font=lang_code.replace('-', '_').lower(),
        strings="\n".join(sorted(strings)),
        sounds="\n".join(sorted(sounds))
    )

    # 写入文件
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, help="输入JSON文件路径")
    parser.add_argument("--output", required=True, help="输出头文件路径")
    args = parser.parse_args()

    generate_header(args.input, args.output)