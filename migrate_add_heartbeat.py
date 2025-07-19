#!/usr/bin/env python3
"""
数据库迁移：添加心跳字段到用户会话表
"""
import sys
import logging
from database import init_database_manager, get_db_manager

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

def migrate_add_heartbeat():
    """添加心跳字段到用户会话表"""
    try:
        # 初始化数据库连接
        init_database_manager()
        db = get_db_manager()
        
        logger.info("开始数据库迁移：添加心跳字段")
        
        # 检查字段是否已存在
        check_sql = """
            SELECT COLUMN_NAME 
            FROM INFORMATION_SCHEMA.COLUMNS 
            WHERE TABLE_SCHEMA = DATABASE() 
            AND TABLE_NAME = 'user_sessions' 
            AND COLUMN_NAME = 'last_heartbeat'
        """
        
        result = db.execute_query(check_sql)
        
        if result:
            logger.info("last_heartbeat字段已存在，跳过迁移")
            return True
        
        # 添加心跳字段
        alter_sql = """
            ALTER TABLE user_sessions 
            ADD COLUMN last_heartbeat DATETIME DEFAULT CURRENT_TIMESTAMP 
            AFTER expires_at
        """
        
        db.execute_update(alter_sql)
        logger.info("成功添加last_heartbeat字段")
        
        # 添加索引
        index_sql = """
            ALTER TABLE user_sessions 
            ADD INDEX idx_last_heartbeat (last_heartbeat)
        """
        
        try:
            db.execute_update(index_sql)
            logger.info("成功添加last_heartbeat索引")
        except Exception as e:
            if "Duplicate key name" in str(e):
                logger.info("last_heartbeat索引已存在")
            else:
                logger.warning(f"添加索引失败: {e}")
        
        # 更新现有会话的心跳时间
        update_sql = """
            UPDATE user_sessions 
            SET last_heartbeat = created_at 
            WHERE last_heartbeat IS NULL
        """
        
        affected_rows = db.execute_update(update_sql)
        logger.info(f"更新了{affected_rows}个现有会话的心跳时间")
        
        logger.info("数据库迁移完成")
        return True
        
    except Exception as e:
        logger.error(f"数据库迁移失败: {e}")
        return False

if __name__ == "__main__":
    if migrate_add_heartbeat():
        print("✅ 数据库迁移成功完成")
        sys.exit(0)
    else:
        print("❌ 数据库迁移失败")
        sys.exit(1)
