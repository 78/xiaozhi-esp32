#!/usr/bin/env python3
import pymysql
import configparser
import logging
import sys
from datetime import datetime

# 设置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

def create_database_and_tables():
    """创建数据库和表"""
    
    # 读取配置
    config = configparser.ConfigParser()
    config.read('config.ini', encoding='utf-8')
    
    # 数据库配置
    db_config = {
        'host': config.get('database', 'host'),
        'port': config.getint('database', 'port'),
        'user': config.get('database', 'username'),
        'password': config.get('database', 'password'),
        'charset': config.get('database', 'charset')
    }
    
    database_name = config.get('database', 'database')
    
    try:
        # 连接MySQL服务器（不指定数据库）
        logger.info("连接MySQL服务器...")
        connection = pymysql.connect(**db_config)
        cursor = connection.cursor()
        
        # 创建数据库
        logger.info(f"创建数据库: {database_name}")
        cursor.execute(f"CREATE DATABASE IF NOT EXISTS `{database_name}` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci")
        
        # 选择数据库
        cursor.execute(f"USE `{database_name}`")
        
        # 创建用户表
        logger.info("创建用户表...")

        # 先删除表（如果存在）
        cursor.execute("DROP TABLE IF EXISTS `user_statistics`")
        cursor.execute("DROP TABLE IF EXISTS `flash_records`")
        cursor.execute("DROP TABLE IF EXISTS `user_sessions`")
        cursor.execute("DROP TABLE IF EXISTS `users`")

        users_table = """
        CREATE TABLE `users` (
            `id` int(11) NOT NULL AUTO_INCREMENT,
            `username` varchar(50) NOT NULL,
            `password_hash` varchar(255) NOT NULL,
            `email` varchar(100) DEFAULT '',
            `real_name` varchar(100) DEFAULT '',
            `status` enum('active','banned','deleted') DEFAULT 'active',
            `created_at` datetime NOT NULL,
            `updated_at` datetime NOT NULL,
            `last_login_at` datetime DEFAULT NULL,
            PRIMARY KEY (`id`),
            UNIQUE KEY `uk_username` (`username`),
            KEY `idx_status` (`status`),
            KEY `idx_created_at` (`created_at`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
        """
        cursor.execute(users_table)
        
        # 创建用户会话表
        logger.info("创建用户会话表...")
        sessions_table = """
        CREATE TABLE `user_sessions` (
            `id` int(11) NOT NULL AUTO_INCREMENT,
            `user_id` int(11) NOT NULL,
            `session_token` varchar(255) NOT NULL,
            `ip_address` varchar(45) DEFAULT NULL,
            `user_agent` varchar(500) DEFAULT NULL,
            `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
            `expires_at` datetime NOT NULL,
            `is_active` tinyint(1) DEFAULT 1,
            PRIMARY KEY (`id`),
            UNIQUE KEY `uk_session_token` (`session_token`),
            KEY `idx_user_id` (`user_id`),
            KEY `idx_expires_at` (`expires_at`),
            KEY `idx_is_active` (`is_active`),
            FOREIGN KEY (`user_id`) REFERENCES `users`(`id`) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
        """
        cursor.execute(sessions_table)
        
        # 创建烧录记录表
        logger.info("创建烧录记录表...")
        flash_records_table = """
        CREATE TABLE `flash_records` (
            `id` int(11) NOT NULL AUTO_INCREMENT,
            `user_id` int(11) NOT NULL,
            `device_id` varchar(100) NOT NULL,
            `device_mac` varchar(17) DEFAULT '',
            `device_type` varchar(50) DEFAULT '',
            `device_version` varchar(20) DEFAULT '',
            `flash_status` enum('in_progress','success','failed') DEFAULT 'in_progress',
            `start_time` datetime NOT NULL,
            `end_time` datetime DEFAULT NULL,
            `duration` int(11) DEFAULT NULL COMMENT '耗时秒数',
            `error_message` text DEFAULT NULL,
            `ip_address` varchar(45) DEFAULT NULL,
            PRIMARY KEY (`id`),
            KEY `idx_user_id` (`user_id`),
            KEY `idx_device_mac` (`device_mac`),
            KEY `idx_flash_status` (`flash_status`),
            KEY `idx_start_time` (`start_time`),
            FOREIGN KEY (`user_id`) REFERENCES `users`(`id`) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
        """
        cursor.execute(flash_records_table)
        
        # 创建用户统计表
        logger.info("创建用户统计表...")
        user_statistics_table = """
        CREATE TABLE `user_statistics` (
            `id` int(11) NOT NULL AUTO_INCREMENT,
            `user_id` int(11) NOT NULL,
            `total_flashes` int(11) DEFAULT 0,
            `successful_flashes` int(11) DEFAULT 0,
            `failed_flashes` int(11) DEFAULT 0,
            `total_devices` int(11) DEFAULT 0,
            `last_flash_at` datetime DEFAULT NULL,
            `updated_at` datetime NOT NULL,
            PRIMARY KEY (`id`),
            UNIQUE KEY `uk_user_id` (`user_id`),
            FOREIGN KEY (`user_id`) REFERENCES `users`(`id`) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
        """
        cursor.execute(user_statistics_table)
        
        # 提交事务
        connection.commit()
        logger.info("数据库表创建完成")
        
        return connection, cursor
        
    except Exception as e:
        logger.error(f"创建数据库失败: {e}")
        return None, None

def create_admin_user(connection, cursor):
    """创建默认管理员用户"""
    try:
        # 读取配置
        config = configparser.ConfigParser()
        config.read('config.ini', encoding='utf-8')
        
        admin_username = config.get('app', 'admin_username')
        admin_password = config.get('app', 'admin_password')
        
        # 检查管理员是否已存在
        cursor.execute("SELECT id FROM users WHERE username = %s", (admin_username,))
        if cursor.fetchone():
            logger.info(f"管理员用户 {admin_username} 已存在")
            return True
        
        # 创建密码哈希
        import bcrypt
        password_hash = bcrypt.hashpw(admin_password.encode('utf-8'), bcrypt.gensalt()).decode('utf-8')
        
        # 插入管理员用户
        insert_sql = """
            INSERT INTO users (username, password_hash, email, real_name, status, created_at, updated_at)
            VALUES (%s, %s, %s, %s, %s, %s, %s)
        """
        cursor.execute(insert_sql, (
            admin_username, password_hash, 'admin@xiaozhi.com', '系统管理员', 
            'active', datetime.now(), datetime.now()
        ))
        
        user_id = cursor.lastrowid
        
        # 创建用户统计记录
        stats_sql = """
            INSERT INTO user_statistics (user_id, total_flashes, successful_flashes, 
                                       failed_flashes, total_devices, updated_at)
            VALUES (%s, 0, 0, 0, 0, %s)
        """
        cursor.execute(stats_sql, (user_id, datetime.now()))
        
        connection.commit()
        logger.info(f"管理员用户创建成功: {admin_username}")
        logger.info(f"默认密码: {admin_password}")
        logger.warning("请及时修改默认密码！")
        
        return True
        
    except Exception as e:
        logger.error(f"创建管理员用户失败: {e}")
        return False

def main():
    """主函数"""
    logger.info("开始初始化数据库...")
    
    # 检查配置文件
    try:
        config = configparser.ConfigParser()
        config.read('config.ini', encoding='utf-8')
    except Exception as e:
        logger.error(f"读取配置文件失败: {e}")
        logger.error("请确保 config.ini 文件存在且格式正确")
        sys.exit(1)
    
    # 创建数据库和表
    connection, cursor = create_database_and_tables()
    if not connection:
        logger.error("数据库初始化失败")
        sys.exit(1)
    
    # 创建管理员用户
    if not create_admin_user(connection, cursor):
        logger.error("创建管理员用户失败")
        sys.exit(1)
    
    # 关闭连接
    cursor.close()
    connection.close()
    
    logger.info("数据库初始化完成！")
    logger.info("现在可以启动应用程序了")

if __name__ == "__main__":
    main()
