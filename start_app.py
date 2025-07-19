#!/usr/bin/env python3
"""
启动应用程序
"""
import sys
import os

def main():
    """主函数"""
    print("=" * 60)
    print("小乔智能设备烧录工具 - 用户管理版本")
    print("=" * 60)
    
    try:
        # 检查必要的文件
        required_files = [
            'config.ini',
            'database.py',
            'auth.py',
            'user_manager.py',
            'flash_logger.py',
            'login_dialog.py',
            'user_management_dialog.py',
            'shaolu_ui_multi.py'
        ]
        
        missing_files = []
        for file in required_files:
            if not os.path.exists(file):
                missing_files.append(file)
        
        if missing_files:
            print("❌ 缺少必要文件:")
            for file in missing_files:
                print(f"  - {file}")
            return 1
        
        print("✅ 所有必要文件检查通过")
        
        # 测试数据库连接
        print("🔍 测试数据库连接...")
        from database import init_database_manager, get_db_manager
        
        init_database_manager()
        db_manager = get_db_manager()
        
        if not db_manager.test_connection():
            print("❌ 数据库连接失败")
            return 1
        
        print("✅ 数据库连接正常")
        
        # 启动应用程序
        print("🚀 启动应用程序...")
        from shaolu_ui_multi import run_multi_device_ui
        
        return run_multi_device_ui()
        
    except ImportError as e:
        print(f"❌ 模块导入失败: {e}")
        print("请确保已安装所有依赖: pip install -r requirements.txt")
        return 1
    except Exception as e:
        print(f"❌ 启动失败: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
