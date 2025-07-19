#!/usr/bin/env python3
"""
系统状态检查
"""
import sys
import os
import configparser

def check_system_status():
    """检查系统状态"""
    print("=" * 60)
    print("小乔智能设备烧录工具 - 系统状态检查")
    print("=" * 60)
    
    status = True
    
    # 1. 检查Python版本
    print("1. Python版本检查:")
    if sys.version_info >= (3, 7):
        print(f"   ✅ Python {sys.version.split()[0]} (满足要求)")
    else:
        print(f"   ❌ Python {sys.version.split()[0]} (需要3.7+)")
        status = False
    
    # 2. 检查必要文件
    print("\n2. 必要文件检查:")
    required_files = [
        'config.ini',
        'database.py',
        'auth.py', 
        'user_manager.py',
        'flash_logger.py',
        'login_dialog.py',
        'user_management_dialog.py',
        'shaolu_ui_multi.py',
        'init_database.py'
    ]
    
    for file in required_files:
        if os.path.exists(file):
            print(f"   ✅ {file}")
        else:
            print(f"   ❌ {file}")
            status = False
    
    # 3. 检查依赖包
    print("\n3. 依赖包检查:")
    required_packages = [
        ('PySide6', 'PySide6'),
        ('PyMySQL', 'pymysql'),
        ('bcrypt', 'bcrypt'),
        ('requests', 'requests'),
        ('pyserial', 'serial')
    ]
    
    for package_name, import_name in required_packages:
        try:
            __import__(import_name)
            print(f"   ✅ {package_name}")
        except ImportError:
            print(f"   ❌ {package_name}")
            status = False
    
    # 4. 检查配置文件
    print("\n4. 配置文件检查:")
    try:
        config = configparser.ConfigParser()
        config.read('config.ini', encoding='utf-8')
        
        # 检查必要的配置节
        required_sections = ['database', 'auth', 'security', 'logging', 'app']
        for section in required_sections:
            if config.has_section(section):
                print(f"   ✅ [{section}] 节存在")
            else:
                print(f"   ❌ [{section}] 节缺失")
                status = False
        
        # 检查数据库配置
        if config.has_section('database'):
            db_host = config.get('database', 'host', fallback='')
            db_name = config.get('database', 'database', fallback='')
            db_user = config.get('database', 'username', fallback='')
            db_pass = config.get('database', 'password', fallback='')
            
            if all([db_host, db_name, db_user, db_pass]):
                print(f"   ✅ 数据库配置完整")
                print(f"      主机: {db_host}")
                print(f"      数据库: {db_name}")
                print(f"      用户: {db_user}")
            else:
                print(f"   ❌ 数据库配置不完整")
                status = False
        
    except Exception as e:
        print(f"   ❌ 配置文件读取失败: {e}")
        status = False
    
    # 5. 检查数据库连接
    print("\n5. 数据库连接检查:")
    try:
        from database import init_database_manager, get_db_manager
        
        init_database_manager()
        db_manager = get_db_manager()
        
        if db_manager.test_connection():
            print("   ✅ 数据库连接正常")
            
            # 检查表是否存在
            tables = db_manager.execute_query("SHOW TABLES")
            expected_tables = ['users', 'user_sessions', 'flash_records', 'user_statistics']
            
            existing_tables = [table[list(table.keys())[0]] for table in tables]
            
            for table in expected_tables:
                if table in existing_tables:
                    print(f"   ✅ 表 {table} 存在")
                else:
                    print(f"   ❌ 表 {table} 不存在")
                    status = False
            
            # 检查管理员用户
            users = db_manager.execute_query("SELECT COUNT(*) as count FROM users WHERE username = 'admin'")
            if users and users[0]['count'] > 0:
                print("   ✅ 管理员用户存在")
            else:
                print("   ❌ 管理员用户不存在")
                status = False
                
        else:
            print("   ❌ 数据库连接失败")
            status = False
            
    except Exception as e:
        print(f"   ❌ 数据库检查失败: {e}")
        status = False
    
    # 6. 检查认证系统
    print("\n6. 认证系统检查:")
    try:
        from auth import get_auth_manager
        
        auth_manager = get_auth_manager()
        
        # 测试密码哈希
        test_password = "test123"
        hashed = auth_manager.hash_password(test_password)
        if auth_manager.verify_password(test_password, hashed):
            print("   ✅ 密码哈希功能正常")
        else:
            print("   ❌ 密码哈希功能异常")
            status = False
        
        # 测试会话令牌生成
        token = auth_manager.generate_session_token()
        if token and len(token) > 20:
            print("   ✅ 会话令牌生成正常")
        else:
            print("   ❌ 会话令牌生成异常")
            status = False
            
    except Exception as e:
        print(f"   ❌ 认证系统检查失败: {e}")
        status = False
    
    # 总结
    print("\n" + "=" * 60)
    if status:
        print("🎉 系统状态检查通过！")
        print("\n可以使用以下命令启动应用程序:")
        print("   python start_app.py")
        print("\n默认管理员账户:")
        print("   用户名: admin")
        print("   密码: admin123456")
        print("\n⚠️  请在首次登录后立即修改默认密码！")
    else:
        print("❌ 系统状态检查失败！")
        print("\n请解决上述问题后重新检查")
    
    print("=" * 60)
    
    return status

if __name__ == "__main__":
    if check_system_status():
        sys.exit(0)
    else:
        sys.exit(1)
