#!/usr/bin/env python3
import argparse
import json
import os

HEADER_TEMPLATE = """// Auto-generated language config
#pragma once

#include <string>
#include <string_view>

namespace Lang {{
    // 语言元数据
    constexpr std::string_view CODE_VIEW = "{lang_code}";
    const std::string CODE = std::string(CODE_VIEW);

    // 字符串资源
    namespace Strings {{
{strings_view}
{strings_string}
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
    strings_view = []
    strings_string = []
    for key, value in data['strings'].items():
        value = value.replace('"', '\\"')
        strings_view.append(f'        constexpr std::string_view {key.upper()}_VIEW = "{value}";')
        strings_string.append(f'        const std::string {key.upper()} = std::string({key.upper()}_VIEW);')

    # 填充模板
    content = HEADER_TEMPLATE.format(
        lang_code=lang_code,
        strings_view="\n".join(sorted(strings_view)),
        strings_string="\n".join(sorted(strings_string))
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