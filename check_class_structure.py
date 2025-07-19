#!/usr/bin/env python3
"""
检查类结构
"""

def check_class_structure():
    """检查MultiDeviceUI类的结构"""
    with open('shaolu_ui_multi.py', 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    in_class = False
    class_indent = 0
    method_count = 0
    
    print("检查MultiDeviceUI类结构:")
    
    for i, line in enumerate(lines, 1):
        stripped = line.strip()
        
        if 'class MultiDeviceUI' in line:
            in_class = True
            class_indent = len(line) - len(line.lstrip())
            print(f"第{i}行: 类定义开始")
            continue
        
        if in_class:
            current_indent = len(line) - len(line.lstrip())
            
            # 如果缩进回到类级别或更少，且不是空行或注释，类定义结束
            if (current_indent <= class_indent and 
                stripped and 
                not stripped.startswith('#') and
                not stripped.startswith('"""') and
                not stripped.startswith("'''") and
                not line.strip().startswith('"""') and
                not line.strip().startswith("'''")):
                print(f"第{i}行: 类定义结束 - {stripped[:50]}")
                break
            
            # 检查方法定义
            if stripped.startswith('def '):
                method_name = stripped.split('(')[0].replace('def ', '')
                print(f"第{i}行: 方法 {method_name}")
                method_count += 1
                
                # 检查特定方法
                if method_name in ['show_user_management', 'set_current_user', 'check_session', 
                                 'force_logout', 'change_current_user_password', 'logout', 'show_about']:
                    print(f"  ✅ 找到用户管理方法: {method_name}")
    
    print(f"\n总共找到 {method_count} 个方法")

if __name__ == "__main__":
    check_class_structure()
