#!/usr/bin/env python3
import logging
import socket
from datetime import datetime, timedelta
from typing import Optional, Dict, Any, List
from database import get_db_manager

logger = logging.getLogger("xiaozhi.flash_logger")

class FlashLogger:
    """烧录记录管理器"""
    
    def __init__(self):
        self.db = get_db_manager()
        logger.info("烧录记录管理器初始化完成")
    
    def get_client_ip(self) -> str:
        """获取客户端IP地址"""
        try:
            hostname = socket.gethostname()
            ip = socket.gethostbyname(hostname)
            return ip
        except:
            return "127.0.0.1"
    
    def start_flash_record(self, user_id: int, device_id: str, device_mac: str = "",
                          device_type: str = "", device_version: str = "") -> Optional[int]:
        """开始烧录记录"""
        try:
            sql = """
                INSERT INTO flash_records (user_id, device_id, device_mac, device_type, 
                                         device_version, flash_status, start_time, ip_address)
                VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
            """
            record_id = self.db.execute_insert(sql, (
                user_id, device_id, device_mac, device_type, device_version,
                'in_progress', datetime.now(), self.get_client_ip()
            ))
            
            logger.info(f"开始烧录记录: 用户{user_id}, 设备{device_id}, 记录ID{record_id}")
            return record_id
            
        except Exception as e:
            logger.error(f"创建烧录记录失败: {e}")
            return None
    
    def finish_flash_record(self, record_id: int, success: bool, 
                           error_message: str = "") -> bool:
        """完成烧录记录"""
        try:
            end_time = datetime.now()
            status = 'success' if success else 'failed'
            
            # 获取开始时间计算耗时
            start_sql = "SELECT start_time, user_id FROM flash_records WHERE id = %s"
            records = self.db.execute_query(start_sql, (record_id,))
            
            if not records:
                logger.error(f"烧录记录 {record_id} 不存在")
                return False
            
            start_time = records[0]['start_time']
            user_id = records[0]['user_id']
            duration = int((end_time - start_time).total_seconds())
            
            # 更新烧录记录
            update_sql = """
                UPDATE flash_records 
                SET flash_status = %s, end_time = %s, duration = %s, error_message = %s
                WHERE id = %s
            """
            self.db.execute_update(update_sql, (status, end_time, duration, error_message, record_id))
            
            # 更新用户统计
            self.update_user_statistics(user_id, success)
            
            logger.info(f"烧录记录完成: 记录ID{record_id}, 状态{status}, 耗时{duration}秒")
            return True
            
        except Exception as e:
            logger.error(f"完成烧录记录失败: {e}")
            return False
    
    def update_user_statistics(self, user_id: int, success: bool):
        """更新用户统计信息"""
        try:
            # 获取当前统计
            stats_sql = """
                SELECT total_flashes, successful_flashes, failed_flashes, total_devices
                FROM user_statistics WHERE user_id = %s
            """
            stats = self.db.execute_query(stats_sql, (user_id,))
            
            if not stats:
                # 如果没有统计记录，创建一个
                insert_sql = """
                    INSERT INTO user_statistics (user_id, total_flashes, successful_flashes, 
                                               failed_flashes, total_devices, last_flash_at, updated_at)
                    VALUES (%s, 1, %s, %s, 1, %s, %s)
                """
                self.db.execute_insert(insert_sql, (
                    user_id, 1 if success else 0, 0 if success else 1,
                    datetime.now(), datetime.now()
                ))
            else:
                # 更新统计
                current_stats = stats[0]
                new_total = current_stats['total_flashes'] + 1
                new_success = current_stats['successful_flashes'] + (1 if success else 0)
                new_failed = current_stats['failed_flashes'] + (0 if success else 1)
                
                # 计算总设备数（去重）
                device_count_sql = """
                    SELECT COUNT(DISTINCT device_mac) as device_count
                    FROM flash_records 
                    WHERE user_id = %s AND device_mac != ''
                """
                device_counts = self.db.execute_query(device_count_sql, (user_id,))
                total_devices = device_counts[0]['device_count'] if device_counts else 0
                
                update_sql = """
                    UPDATE user_statistics 
                    SET total_flashes = %s, successful_flashes = %s, failed_flashes = %s,
                        total_devices = %s, last_flash_at = %s, updated_at = %s
                    WHERE user_id = %s
                """
                self.db.execute_update(update_sql, (
                    new_total, new_success, new_failed, total_devices,
                    datetime.now(), datetime.now(), user_id
                ))
            
            logger.debug(f"用户 {user_id} 统计信息已更新")
            
        except Exception as e:
            logger.error(f"更新用户统计失败: {e}")
    
    def get_user_flash_records(self, user_id: int, limit: int = 100, 
                              offset: int = 0) -> List[Dict[str, Any]]:
        """获取用户烧录记录"""
        try:
            sql = """
                SELECT id, device_id, device_mac, device_type, device_version,
                       flash_status, start_time, end_time, duration, error_message, ip_address
                FROM flash_records 
                WHERE user_id = %s
                ORDER BY start_time DESC
                LIMIT %s OFFSET %s
            """
            return self.db.execute_query(sql, (user_id, limit, offset))
        except Exception as e:
            logger.error(f"获取用户烧录记录失败: {e}")
            return []
    
    def get_all_flash_records(self, limit: int = 100, offset: int = 0,
                             status_filter: str = None) -> List[Dict[str, Any]]:
        """获取所有烧录记录"""
        try:
            if status_filter:
                sql = """
                    SELECT r.id, r.device_id, r.device_mac, r.device_type, r.device_version,
                           r.flash_status, r.start_time, r.end_time, r.duration, 
                           r.error_message, r.ip_address, u.username, u.real_name
                    FROM flash_records r
                    JOIN users u ON r.user_id = u.id
                    WHERE r.flash_status = %s
                    ORDER BY r.start_time DESC
                    LIMIT %s OFFSET %s
                """
                return self.db.execute_query(sql, (status_filter, limit, offset))
            else:
                sql = """
                    SELECT r.id, r.device_id, r.device_mac, r.device_type, r.device_version,
                           r.flash_status, r.start_time, r.end_time, r.duration, 
                           r.error_message, r.ip_address, u.username, u.real_name
                    FROM flash_records r
                    JOIN users u ON r.user_id = u.id
                    ORDER BY r.start_time DESC
                    LIMIT %s OFFSET %s
                """
                return self.db.execute_query(sql, (limit, offset))
        except Exception as e:
            logger.error(f"获取烧录记录失败: {e}")
            return []
    
    def get_flash_statistics(self, days: int = 30) -> Dict[str, Any]:
        """获取烧录统计信息"""
        try:
            start_date = datetime.now() - timedelta(days=days)
            
            # 总体统计
            total_sql = """
                SELECT 
                    COUNT(*) as total_records,
                    SUM(CASE WHEN flash_status = 'success' THEN 1 ELSE 0 END) as successful_records,
                    SUM(CASE WHEN flash_status = 'failed' THEN 1 ELSE 0 END) as failed_records,
                    COUNT(DISTINCT user_id) as active_users,
                    COUNT(DISTINCT device_mac) as unique_devices,
                    AVG(CASE WHEN flash_status = 'success' THEN duration ELSE NULL END) as avg_success_duration
                FROM flash_records 
                WHERE start_time >= %s
            """
            total_stats = self.db.execute_query(total_sql, (start_date,))
            
            # 每日统计
            daily_sql = """
                SELECT 
                    DATE(start_time) as flash_date,
                    COUNT(*) as daily_total,
                    SUM(CASE WHEN flash_status = 'success' THEN 1 ELSE 0 END) as daily_success,
                    SUM(CASE WHEN flash_status = 'failed' THEN 1 ELSE 0 END) as daily_failed
                FROM flash_records 
                WHERE start_time >= %s
                GROUP BY DATE(start_time)
                ORDER BY flash_date DESC
            """
            daily_stats = self.db.execute_query(daily_sql, (start_date,))
            
            # 用户排行
            user_ranking_sql = """
                SELECT 
                    u.username, u.real_name,
                    COUNT(*) as user_total,
                    SUM(CASE WHEN r.flash_status = 'success' THEN 1 ELSE 0 END) as user_success
                FROM flash_records r
                JOIN users u ON r.user_id = u.id
                WHERE r.start_time >= %s
                GROUP BY r.user_id, u.username, u.real_name
                ORDER BY user_total DESC
                LIMIT 10
            """
            user_ranking = self.db.execute_query(user_ranking_sql, (start_date,))
            
            return {
                'total_stats': total_stats[0] if total_stats else {},
                'daily_stats': daily_stats,
                'user_ranking': user_ranking,
                'period_days': days
            }
            
        except Exception as e:
            logger.error(f"获取烧录统计失败: {e}")
            return {}
    
    def search_flash_records(self, keyword: str, user_id: int = None) -> List[Dict[str, Any]]:
        """搜索烧录记录"""
        try:
            if user_id:
                sql = """
                    SELECT id, device_id, device_mac, device_type, device_version,
                           flash_status, start_time, end_time, duration, error_message
                    FROM flash_records 
                    WHERE user_id = %s AND (
                        device_id LIKE %s OR device_mac LIKE %s OR 
                        device_type LIKE %s OR error_message LIKE %s
                    )
                    ORDER BY start_time DESC
                    LIMIT 100
                """
                pattern = f"%{keyword}%"
                return self.db.execute_query(sql, (user_id, pattern, pattern, pattern, pattern))
            else:
                sql = """
                    SELECT r.id, r.device_id, r.device_mac, r.device_type, r.device_version,
                           r.flash_status, r.start_time, r.end_time, r.duration, 
                           r.error_message, u.username, u.real_name
                    FROM flash_records r
                    JOIN users u ON r.user_id = u.id
                    WHERE (
                        r.device_id LIKE %s OR r.device_mac LIKE %s OR 
                        r.device_type LIKE %s OR r.error_message LIKE %s OR
                        u.username LIKE %s OR u.real_name LIKE %s
                    )
                    ORDER BY r.start_time DESC
                    LIMIT 100
                """
                pattern = f"%{keyword}%"
                return self.db.execute_query(sql, (pattern, pattern, pattern, pattern, pattern, pattern))
                
        except Exception as e:
            logger.error(f"搜索烧录记录失败: {e}")
            return []

# 全局烧录记录管理器实例
_flash_logger = None

def get_flash_logger() -> FlashLogger:
    """获取烧录记录管理器实例"""
    global _flash_logger
    if _flash_logger is None:
        _flash_logger = FlashLogger()
    return _flash_logger
