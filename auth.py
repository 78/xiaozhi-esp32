#!/usr/bin/env python3
import bcrypt
import uuid
import hashlib
import logging
import configparser
import time
import socket
from datetime import datetime, timedelta
from typing import Optional, Dict, Any
from database import get_db_manager

logger = logging.getLogger("xiaozhi.auth")

class AuthManager:
    """用户认证管理器"""
    
    def __init__(self, config_file: str = "config.ini"):
        self.config = configparser.ConfigParser()
        self.config.read(config_file, encoding='utf-8')
        
        # 认证配置
        self.session_timeout = self.config.getint('auth', 'session_timeout')
        self.password_min_length = self.config.getint('auth', 'password_min_length')
        self.max_login_attempts = self.config.getint('auth', 'max_login_attempts')
        self.lockout_duration = self.config.getint('auth', 'lockout_duration')
        self.secret_key = self.config.get('auth', 'secret_key')
        
        # 安全配置
        self.enable_ip_logging = self.config.getboolean('security', 'enable_ip_logging')
        self.force_single_session = self.config.getboolean('security', 'force_single_session')
        
        self.db = get_db_manager()
        
        # 登录失败记录 {username: {'count': int, 'last_attempt': timestamp}}
        self.login_attempts = {}
        
        logger.info("用户认证管理器初始化完成")
    
    def hash_password(self, password: str) -> str:
        """密码哈希"""
        salt = bcrypt.gensalt()
        hashed = bcrypt.hashpw(password.encode('utf-8'), salt)
        return hashed.decode('utf-8')
    
    def verify_password(self, password: str, hashed: str) -> bool:
        """验证密码"""
        try:
            return bcrypt.checkpw(password.encode('utf-8'), hashed.encode('utf-8'))
        except Exception as e:
            logger.error(f"密码验证失败: {e}")
            return False
    
    def generate_session_token(self) -> str:
        """生成会话令牌"""
        return str(uuid.uuid4())
    
    def get_client_ip(self) -> str:
        """获取客户端IP地址"""
        try:
            # 获取本机IP
            hostname = socket.gethostname()
            ip = socket.gethostbyname(hostname)
            return ip
        except:
            return "127.0.0.1"
    
    def is_user_locked(self, username: str) -> bool:
        """检查用户是否被锁定"""
        if username not in self.login_attempts:
            return False
        
        attempt_info = self.login_attempts[username]
        if attempt_info['count'] >= self.max_login_attempts:
            # 检查锁定是否过期
            if time.time() - attempt_info['last_attempt'] > self.lockout_duration:
                # 锁定过期，重置计数
                del self.login_attempts[username]
                return False
            return True
        return False
    
    def record_login_attempt(self, username: str, success: bool):
        """记录登录尝试"""
        current_time = time.time()
        
        if success:
            # 登录成功，清除失败记录
            if username in self.login_attempts:
                del self.login_attempts[username]
        else:
            # 登录失败，增加计数
            if username not in self.login_attempts:
                self.login_attempts[username] = {'count': 0, 'last_attempt': current_time}
            
            self.login_attempts[username]['count'] += 1
            self.login_attempts[username]['last_attempt'] = current_time
    
    def login(self, username: str, password: str) -> Optional[Dict[str, Any]]:
        """用户登录"""
        try:
            # 检查用户是否被锁定
            if self.is_user_locked(username):
                logger.warning(f"用户 {username} 因多次登录失败被锁定")
                return None
            
            # 查询用户信息
            user_sql = """
                SELECT id, username, password_hash, email, real_name, status, last_login_at
                FROM users 
                WHERE username = %s
            """
            users = self.db.execute_query(user_sql, (username,))
            
            if not users:
                self.record_login_attempt(username, False)
                logger.warning(f"用户 {username} 不存在")
                return None
            
            user = users[0]
            
            # 检查用户状态
            if user['status'] != 'active':
                self.record_login_attempt(username, False)
                logger.warning(f"用户 {username} 已被禁用")
                return None
            
            # 验证密码
            if not self.verify_password(password, user['password_hash']):
                self.record_login_attempt(username, False)
                logger.warning(f"用户 {username} 密码错误")
                return None
            
            # 登录成功
            self.record_login_attempt(username, True)
            
            # 如果启用单点登录，先清除该用户的其他会话
            if self.force_single_session:
                self.logout_user_all_sessions(user['id'])
            
            # 创建会话（永不过期，除非主动登出或被强制下线）
            session_token = self.generate_session_token()
            ip_address = self.get_client_ip() if self.enable_ip_logging else None
            # 设置一个很远的过期时间（100年后），实际上永不过期
            expires_at = datetime.now() + timedelta(days=36500)

            session_sql = """
                INSERT INTO user_sessions (user_id, session_token, ip_address, user_agent, expires_at, last_heartbeat, is_active)
                VALUES (%s, %s, %s, %s, %s, %s, %s)
            """
            self.db.execute_insert(session_sql, (
                user['id'], session_token, ip_address, "ESP32烧录工具", expires_at, datetime.now(), True
            ))
            
            # 更新用户最后登录时间
            update_sql = "UPDATE users SET last_login_at = %s WHERE id = %s"
            self.db.execute_update(update_sql, (datetime.now(), user['id']))
            
            logger.info(f"用户 {username} 登录成功")
            
            return {
                'user_id': user['id'],
                'username': user['username'],
                'email': user['email'],
                'real_name': user['real_name'],
                'session_token': session_token,
                'expires_at': expires_at
            }
            
        except Exception as e:
            logger.error(f"用户登录失败: {e}")
            return None
    
    def verify_session(self, session_token: str) -> Optional[Dict[str, Any]]:
        """验证会话"""
        try:
            session_sql = """
                SELECT s.user_id, s.is_active, s.last_heartbeat,
                       u.username, u.email, u.real_name, u.status
                FROM user_sessions s
                JOIN users u ON s.user_id = u.id
                WHERE s.session_token = %s AND s.is_active = 1
            """
            sessions = self.db.execute_query(session_sql, (session_token,))

            if not sessions:
                return None

            session = sessions[0]

            # 只检查用户状态，不检查会话过期
            if session['status'] != 'active':
                self.logout_session(session_token)
                return None

            # 更新心跳时间
            self.update_heartbeat(session_token)

            return {
                'user_id': session['user_id'],
                'username': session['username'],
                'email': session['email'],
                'real_name': session['real_name']
            }

        except Exception as e:
            logger.error(f"会话验证失败: {e}")
            return None

    def update_heartbeat(self, session_token: str):
        """更新会话心跳时间"""
        try:
            sql = "UPDATE user_sessions SET last_heartbeat = %s WHERE session_token = %s AND is_active = 1"
            self.db.execute_update(sql, (datetime.now(), session_token))
        except Exception as e:
            logger.error(f"更新心跳失败: {e}")

    def is_session_alive(self, session_token: str, heartbeat_timeout: int = 300) -> bool:
        """检查会话是否活跃（基于心跳）"""
        try:
            sql = """
                SELECT last_heartbeat
                FROM user_sessions
                WHERE session_token = %s AND is_active = 1
            """
            result = self.db.execute_query(sql, (session_token,))

            if not result:
                return False

            last_heartbeat = result[0]['last_heartbeat']
            if not last_heartbeat:
                return False

            # 检查心跳是否超时（默认5分钟）
            timeout_threshold = datetime.now() - timedelta(seconds=heartbeat_timeout)
            return last_heartbeat > timeout_threshold

        except Exception as e:
            logger.error(f"检查会话活跃状态失败: {e}")
            return False
    
    def logout_session(self, session_token: str) -> bool:
        """登出指定会话"""
        try:
            sql = "UPDATE user_sessions SET is_active = 0 WHERE session_token = %s"
            affected_rows = self.db.execute_update(sql, (session_token,))
            return affected_rows > 0
        except Exception as e:
            logger.error(f"登出会话失败: {e}")
            return False
    
    def logout_user_all_sessions(self, user_id: int) -> bool:
        """登出用户的所有会话"""
        try:
            sql = "UPDATE user_sessions SET is_active = 0 WHERE user_id = %s"
            self.db.execute_update(sql, (user_id,))
            logger.info(f"用户 {user_id} 的所有会话已登出")
            return True
        except Exception as e:
            logger.error(f"登出用户所有会话失败: {e}")
            return False
    
    def cleanup_expired_sessions(self):
        """清理过期会话（已禁用自动过期功能）"""
        # 不再自动清理会话，只有主动登出或管理员强制下线才会清理
        logger.debug("会话自动过期功能已禁用，会话将保持活跃状态")

    def cleanup_inactive_sessions(self, heartbeat_timeout: int = 300):
        """清理无心跳的会话"""
        try:
            timeout_threshold = datetime.now() - timedelta(seconds=heartbeat_timeout)
            sql = """
                UPDATE user_sessions
                SET is_active = 0
                WHERE is_active = 1 AND last_heartbeat < %s
            """
            affected_rows = self.db.execute_update(sql, (timeout_threshold,))
            if affected_rows > 0:
                logger.info(f"清理了 {affected_rows} 个无心跳的会话")
            return affected_rows
        except Exception as e:
            logger.error(f"清理无心跳会话失败: {e}")
            return 0
    
    def get_active_sessions(self, user_id: int = None, heartbeat_timeout: int = 300) -> list:
        """获取活跃会话（基于心跳检测）"""
        try:
            # 计算心跳超时阈值
            timeout_threshold = datetime.now() - timedelta(seconds=heartbeat_timeout)

            if user_id:
                sql = """
                    SELECT s.session_token, s.ip_address, s.created_at, s.expires_at, s.last_heartbeat,
                           u.username, u.real_name
                    FROM user_sessions s
                    JOIN users u ON s.user_id = u.id
                    WHERE s.user_id = %s AND s.is_active = 1 AND s.last_heartbeat > %s
                """
                return self.db.execute_query(sql, (user_id, timeout_threshold))
            else:
                sql = """
                    SELECT s.session_token, s.ip_address, s.created_at, s.expires_at, s.last_heartbeat,
                           u.username, u.real_name
                    FROM user_sessions s
                    JOIN users u ON s.user_id = u.id
                    WHERE s.is_active = 1 AND s.last_heartbeat > %s
                """
                return self.db.execute_query(sql, (timeout_threshold,))
        except Exception as e:
            logger.error(f"获取活跃会话失败: {e}")
            return []

# 全局认证管理器实例
_auth_manager = None

def get_auth_manager() -> AuthManager:
    """获取认证管理器实例"""
    global _auth_manager
    if _auth_manager is None:
        _auth_manager = AuthManager()
    return _auth_manager
