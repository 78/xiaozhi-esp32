#!/usr/bin/env python3
import logging
from datetime import datetime
from typing import Optional, Dict, Any, List
from database import get_db_manager
from auth import get_auth_manager

logger = logging.getLogger("xiaozhi.user_manager")

class UserManager:
    """用户管理器"""
    
    def __init__(self):
        self.db = get_db_manager()
        self.auth = get_auth_manager()
        logger.info("用户管理器初始化完成")
    
    def create_user(self, username: str, password: str, email: str = "", 
                   real_name: str = "", status: str = "active") -> Optional[int]:
        """创建用户"""
        try:
            # 检查用户名是否已存在
            if self.get_user_by_username(username):
                logger.warning(f"用户名 {username} 已存在")
                return None
            
            # 验证密码长度
            if len(password) < self.auth.password_min_length:
                logger.warning(f"密码长度不足，最少需要 {self.auth.password_min_length} 位")
                return None
            
            # 哈希密码
            password_hash = self.auth.hash_password(password)
            
            # 插入用户
            sql = """
                INSERT INTO users (username, password_hash, email, real_name, status, created_at, updated_at)
                VALUES (%s, %s, %s, %s, %s, %s, %s)
            """
            user_id = self.db.execute_insert(sql, (
                username, password_hash, email, real_name, status, 
                datetime.now(), datetime.now()
            ))
            
            # 创建用户统计记录
            stats_sql = """
                INSERT INTO user_statistics (user_id, total_flashes, successful_flashes, 
                                           failed_flashes, total_devices, updated_at)
                VALUES (%s, 0, 0, 0, 0, %s)
            """
            self.db.execute_insert(stats_sql, (user_id, datetime.now()))
            
            logger.info(f"用户 {username} 创建成功，ID: {user_id}")
            return user_id
            
        except Exception as e:
            logger.error(f"创建用户失败: {e}")
            return None
    
    def get_user_by_id(self, user_id: int) -> Optional[Dict[str, Any]]:
        """根据ID获取用户"""
        try:
            sql = """
                SELECT id, username, email, real_name, status, 
                       created_at, updated_at, last_login_at
                FROM users WHERE id = %s
            """
            users = self.db.execute_query(sql, (user_id,))
            return users[0] if users else None
        except Exception as e:
            logger.error(f"获取用户失败: {e}")
            return None
    
    def get_user_by_username(self, username: str) -> Optional[Dict[str, Any]]:
        """根据用户名获取用户"""
        try:
            sql = """
                SELECT id, username, email, real_name, status, 
                       created_at, updated_at, last_login_at
                FROM users WHERE username = %s
            """
            users = self.db.execute_query(sql, (username,))
            return users[0] if users else None
        except Exception as e:
            logger.error(f"获取用户失败: {e}")
            return None
    
    def get_all_users(self, include_inactive: bool = True) -> List[Dict[str, Any]]:
        """获取所有用户"""
        try:
            if include_inactive:
                sql = """
                    SELECT u.id, u.username, u.email, u.real_name, u.status, 
                           u.created_at, u.updated_at, u.last_login_at,
                           s.total_flashes, s.successful_flashes, s.failed_flashes,
                           s.total_devices, s.last_flash_at
                    FROM users u
                    LEFT JOIN user_statistics s ON u.id = s.user_id
                    ORDER BY u.created_at DESC
                """
                return self.db.execute_query(sql)
            else:
                sql = """
                    SELECT u.id, u.username, u.email, u.real_name, u.status, 
                           u.created_at, u.updated_at, u.last_login_at,
                           s.total_flashes, s.successful_flashes, s.failed_flashes,
                           s.total_devices, s.last_flash_at
                    FROM users u
                    LEFT JOIN user_statistics s ON u.id = s.user_id
                    WHERE u.status = 'active'
                    ORDER BY u.created_at DESC
                """
                return self.db.execute_query(sql)
        except Exception as e:
            logger.error(f"获取用户列表失败: {e}")
            return []
    
    def update_user(self, user_id: int, **kwargs) -> bool:
        """更新用户信息"""
        try:
            # 构建更新字段
            update_fields = []
            params = []
            
            allowed_fields = ['email', 'real_name', 'status']
            for field, value in kwargs.items():
                if field in allowed_fields:
                    update_fields.append(f"{field} = %s")
                    params.append(value)
            
            if not update_fields:
                return False
            
            # 添加更新时间
            update_fields.append("updated_at = %s")
            params.append(datetime.now())
            params.append(user_id)
            
            sql = f"UPDATE users SET {', '.join(update_fields)} WHERE id = %s"
            affected_rows = self.db.execute_update(sql, tuple(params))
            
            if affected_rows > 0:
                logger.info(f"用户 {user_id} 信息更新成功")
                return True
            return False
            
        except Exception as e:
            logger.error(f"更新用户信息失败: {e}")
            return False
    
    def change_password(self, user_id: int, new_password: str) -> bool:
        """修改用户密码"""
        try:
            # 验证密码长度
            if len(new_password) < self.auth.password_min_length:
                logger.warning(f"密码长度不足，最少需要 {self.auth.password_min_length} 位")
                return False
            
            # 哈希新密码
            password_hash = self.auth.hash_password(new_password)
            
            sql = "UPDATE users SET password_hash = %s, updated_at = %s WHERE id = %s"
            affected_rows = self.db.execute_update(sql, (password_hash, datetime.now(), user_id))
            
            if affected_rows > 0:
                # 强制用户重新登录
                self.auth.logout_user_all_sessions(user_id)
                logger.info(f"用户 {user_id} 密码修改成功")
                return True
            return False
            
        except Exception as e:
            logger.error(f"修改密码失败: {e}")
            return False
    
    def ban_user(self, user_id: int) -> bool:
        """封禁用户"""
        try:
            sql = "UPDATE users SET status = 'banned', updated_at = %s WHERE id = %s"
            affected_rows = self.db.execute_update(sql, (datetime.now(), user_id))
            
            if affected_rows > 0:
                # 强制用户下线
                self.auth.logout_user_all_sessions(user_id)
                logger.info(f"用户 {user_id} 已被封禁")
                return True
            return False
            
        except Exception as e:
            logger.error(f"封禁用户失败: {e}")
            return False
    
    def unban_user(self, user_id: int) -> bool:
        """解封用户"""
        try:
            sql = "UPDATE users SET status = 'active', updated_at = %s WHERE id = %s"
            affected_rows = self.db.execute_update(sql, (datetime.now(), user_id))
            
            if affected_rows > 0:
                logger.info(f"用户 {user_id} 已解封")
                return True
            return False
            
        except Exception as e:
            logger.error(f"解封用户失败: {e}")
            return False
    
    def delete_user(self, user_id: int) -> bool:
        """删除用户（软删除）"""
        try:
            # 先强制下线
            self.auth.logout_user_all_sessions(user_id)
            
            # 软删除用户
            sql = "UPDATE users SET status = 'deleted', updated_at = %s WHERE id = %s"
            affected_rows = self.db.execute_update(sql, (datetime.now(), user_id))
            
            if affected_rows > 0:
                logger.info(f"用户 {user_id} 已删除")
                return True
            return False
            
        except Exception as e:
            logger.error(f"删除用户失败: {e}")
            return False
    
    def force_logout_user(self, user_id: int) -> bool:
        """强制用户下线"""
        try:
            result = self.auth.logout_user_all_sessions(user_id)
            if result:
                logger.info(f"用户 {user_id} 已被强制下线")
            return result
        except Exception as e:
            logger.error(f"强制下线失败: {e}")
            return False
    
    def get_user_statistics(self, user_id: int) -> Optional[Dict[str, Any]]:
        """获取用户统计信息"""
        try:
            sql = """
                SELECT total_flashes, successful_flashes, failed_flashes,
                       total_devices, last_flash_at, updated_at
                FROM user_statistics WHERE user_id = %s
            """
            stats = self.db.execute_query(sql, (user_id,))
            return stats[0] if stats else None
        except Exception as e:
            logger.error(f"获取用户统计失败: {e}")
            return None
    
    def search_users(self, keyword: str) -> List[Dict[str, Any]]:
        """搜索用户"""
        try:
            sql = """
                SELECT u.id, u.username, u.email, u.real_name, u.status, 
                       u.created_at, u.updated_at, u.last_login_at,
                       s.total_flashes, s.successful_flashes, s.failed_flashes
                FROM users u
                LEFT JOIN user_statistics s ON u.id = s.user_id
                WHERE u.username LIKE %s OR u.email LIKE %s OR u.real_name LIKE %s
                ORDER BY u.created_at DESC
            """
            pattern = f"%{keyword}%"
            return self.db.execute_query(sql, (pattern, pattern, pattern))
        except Exception as e:
            logger.error(f"搜索用户失败: {e}")
            return []

# 全局用户管理器实例
_user_manager = None

def get_user_manager() -> UserManager:
    """获取用户管理器实例"""
    global _user_manager
    if _user_manager is None:
        _user_manager = UserManager()
    return _user_manager
