#!/usr/bin/env python3
"""
小乔智能设备烧录工具 - 用户管理系统快速设置脚本
"""
import os
import sys
import subprocess
import configparser
import logging

# 设置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

def check_python_version():
    """检查Python版本"""
    if sys.version_info < (3, 7):
        logger.error("需要Python 3.7或更高版本")
        return False
    logger.info(f"Python版本检查通过: {sys.version}")
    return True

def install_dependencies():
    """安装依赖包"""
    logger.info("开始安装依赖包...")
    
    try:
        # 检查pip
        subprocess.run([sys.executable, "-m", "pip", "--version"], check=True, capture_output=True)
        
        # 安装依赖
        requirements = [
            "PySide6>=6.5.0",
            "requests>=2.28.0", 
            "pyserial>=3.5",
            "PyMySQL>=1.0.2",
            "bcrypt>=4.0.1"
        ]
        
        for req in requirements:
            logger.info(f"安装 {req}...")
            result = subprocess.run([sys.executable, "-m", "pip", "install", req], 
                                  capture_output=True, text=True)
            if result.returncode != 0:
                logger.error(f"安装 {req} 失败: {result.stderr}")
                return False
            
        logger.info("所有依赖包安装完成")
        return True
        
    except subprocess.CalledProcessError as e:
        logger.error(f"安装依赖失败: {e}")
        return False
    except Exception as e:
        logger.error(f"安装依赖时发生错误: {e}")
        return False

def create_config_file():
    """创建配置文件"""
    logger.info("创建配置文件...")
    
    if os.path.exists("config.ini"):
        logger.info("配置文件已存在，跳过创建")
        return True
    
    try:
        config = configparser.ConfigParser()
        
        # 数据库配置
        config.add_section('database')
        config.set('database', 'host', 'localhost')
        config.set('database', 'port', '3306')
        config.set('database', 'database', 'xiaozhi_esp32')
        config.set('database', 'username', 'root')
        config.set('database', 'password', 'your_password_here')
        config.set('database', 'charset', 'utf8mb4')
        config.set('database', 'pool_size', '10')
        config.set('database', 'pool_recycle', '3600')
        
        # 认证配置
        config.add_section('auth')
        config.set('auth', 'session_timeout', '28800')
        config.set('auth', 'password_min_length', '6')
        config.set('auth', 'max_login_attempts', '5')
        config.set('auth', 'lockout_duration', '300')
        config.set('auth', 'secret_key', 'your_secret_key_here_change_this')
        
        # 安全配置
        config.add_section('security')
        config.set('security', 'enable_ip_logging', 'true')
        config.set('security', 'enable_session_cleanup', 'true')
        config.set('security', 'session_cleanup_interval', '3600')
        config.set('security', 'force_single_session', 'false')
        
        # 日志配置
        config.add_section('logging')
        config.set('logging', 'log_level', 'INFO')
        config.set('logging', 'log_file', 'xiaozhi_esp32.log')
        config.set('logging', 'max_log_size', '10485760')
        config.set('logging', 'backup_count', '5')
        
        # 应用配置
        config.add_section('app')
        config.set('app', 'app_name', '小乔智能设备烧录工具')
        config.set('app', 'version', '1.0.0')
        config.set('app', 'admin_username', 'admin')
        config.set('app', 'admin_password', 'admin123456')
        
        # 写入配置文件
        with open('config.ini', 'w', encoding='utf-8') as f:
            config.write(f)
        
        logger.info("配置文件创建成功")
        return True
        
    except Exception as e:
        logger.error(f"创建配置文件失败: {e}")
        return False

def check_mysql_connection():
    """检查MySQL连接"""
    logger.info("检查MySQL连接...")
    
    try:
        import pymysql
        
        # 读取配置
        config = configparser.ConfigParser()
        config.read('config.ini', encoding='utf-8')
        
        # 获取数据库配置
        host = config.get('database', 'host')
        port = config.getint('database', 'port')
        username = config.get('database', 'username')
        password = config.get('database', 'password')
        
        if password == 'your_password_here':
            logger.warning("请先在config.ini中配置正确的数据库密码")
            return False
        
        # 尝试连接
        connection = pymysql.connect(
            host=host,
            port=port,
            user=username,
            password=password,
            charset='utf8mb4'
        )
        connection.close()
        
        logger.info("MySQL连接测试成功")
        return True
        
    except ImportError:
        logger.error("PyMySQL未安装，请先安装依赖")
        return False
    except Exception as e:
        logger.error(f"MySQL连接失败: {e}")
        logger.error("请检查MySQL服务是否启动，以及config.ini中的数据库配置是否正确")
        return False

def initialize_database():
    """初始化数据库"""
    logger.info("初始化数据库...")
    
    try:
        # 运行数据库初始化脚本
        result = subprocess.run([sys.executable, "init_database.py"], 
                              capture_output=True, text=True)
        
        if result.returncode == 0:
            logger.info("数据库初始化成功")
            logger.info("默认管理员账户:")
            logger.info("  用户名: admin")
            logger.info("  密码: admin123456")
            logger.warning("请在首次登录后立即修改默认密码！")
            return True
        else:
            logger.error(f"数据库初始化失败: {result.stderr}")
            return False
            
    except Exception as e:
        logger.error(f"数据库初始化时发生错误: {e}")
        return False

def test_application():
    """测试应用程序"""
    logger.info("测试应用程序...")
    
    try:
        # 测试导入主要模块
        import database
        import auth
        import user_manager
        import flash_logger
        import login_dialog
        
        logger.info("所有模块导入成功")
        
        # 测试数据库连接
        db_manager = database.get_db_manager()
        if db_manager.test_connection():
            logger.info("数据库连接测试成功")
            return True
        else:
            logger.error("数据库连接测试失败")
            return False
            
    except ImportError as e:
        logger.error(f"模块导入失败: {e}")
        return False
    except Exception as e:
        logger.error(f"应用程序测试失败: {e}")
        return False

def main():
    """主函数"""
    logger.info("=" * 60)
    logger.info("小乔智能设备烧录工具 - 用户管理系统快速设置")
    logger.info("=" * 60)
    
    # 检查Python版本
    if not check_python_version():
        return 1
    
    # 安装依赖
    if not install_dependencies():
        logger.error("依赖安装失败，请手动安装")
        return 1
    
    # 创建配置文件
    if not create_config_file():
        logger.error("配置文件创建失败")
        return 1
    
    # 提示用户配置数据库
    logger.info("=" * 60)
    logger.info("请按照以下步骤配置数据库:")
    logger.info("1. 确保MySQL服务已启动")
    logger.info("2. 编辑config.ini文件，设置正确的数据库密码")
    logger.info("3. 重新运行此脚本继续设置")
    logger.info("=" * 60)
    
    # 检查MySQL连接
    if not check_mysql_connection():
        logger.error("请先配置数据库连接")
        return 1
    
    # 初始化数据库
    if not initialize_database():
        logger.error("数据库初始化失败")
        return 1
    
    # 测试应用程序
    if not test_application():
        logger.error("应用程序测试失败")
        return 1
    
    # 设置完成
    logger.info("=" * 60)
    logger.info("🎉 用户管理系统设置完成！")
    logger.info("")
    logger.info("现在可以启动应用程序:")
    logger.info("  python shaolu_ui_multi.py")
    logger.info("")
    logger.info("默认管理员账户:")
    logger.info("  用户名: admin")
    logger.info("  密码: admin123456")
    logger.info("")
    logger.warning("⚠️  请在首次登录后立即修改默认密码！")
    logger.info("=" * 60)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
