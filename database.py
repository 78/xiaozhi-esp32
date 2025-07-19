#!/usr/bin/env python3
import pymysql
import logging
import configparser
import threading
import time
from contextlib import contextmanager
from typing import Optional, Dict, Any, List, Tuple
from pymysql.cursors import DictCursor

logger = logging.getLogger("xiaozhi.database")

class DatabaseManager:
    """数据库管理器，提供连接池和基础操作"""
    
    def __init__(self, config_file: str = "config.ini"):
        self.config = configparser.ConfigParser()
        self.config.read(config_file, encoding='utf-8')
        
        # 数据库配置
        self.db_config = {
            'host': self.config.get('database', 'host'),
            'port': self.config.getint('database', 'port'),
            'user': self.config.get('database', 'username'),
            'password': self.config.get('database', 'password'),
            'database': self.config.get('database', 'database'),
            'charset': self.config.get('database', 'charset'),
            'autocommit': True,
            'cursorclass': DictCursor
        }
        
        # 连接池配置
        self.pool_size = self.config.getint('database', 'pool_size')
        self.pool_recycle = self.config.getint('database', 'pool_recycle')
        
        # 连接池
        self._pool = []
        self._pool_lock = threading.Lock()
        self._last_cleanup = time.time()
        
        # 初始化连接池
        self._init_pool()
        
        logger.info(f"数据库管理器初始化完成，连接池大小: {self.pool_size}")
    
    def _init_pool(self):
        """初始化连接池"""
        try:
            for _ in range(self.pool_size):
                conn = self._create_connection()
                if conn:
                    self._pool.append({
                        'connection': conn,
                        'created_at': time.time(),
                        'in_use': False
                    })
            logger.info(f"连接池初始化成功，创建了 {len(self._pool)} 个连接")
        except Exception as e:
            logger.error(f"连接池初始化失败: {e}")
            raise
    
    def _create_connection(self):
        """创建新的数据库连接"""
        try:
            conn = pymysql.connect(**self.db_config)
            return conn
        except Exception as e:
            logger.error(f"创建数据库连接失败: {e}")
            return None
    
    @contextmanager
    def get_connection(self):
        """获取数据库连接（上下文管理器）"""
        conn_info = None
        try:
            # 获取连接
            conn_info = self._get_connection_from_pool()
            if not conn_info:
                raise Exception("无法获取数据库连接")
            
            yield conn_info['connection']
            
        except Exception as e:
            logger.error(f"数据库操作异常: {e}")
            raise
        finally:
            # 归还连接
            if conn_info:
                self._return_connection_to_pool(conn_info)
    
    def _get_connection_from_pool(self):
        """从连接池获取连接"""
        with self._pool_lock:
            # 清理过期连接
            self._cleanup_expired_connections()
            
            # 查找可用连接
            for conn_info in self._pool:
                if not conn_info['in_use']:
                    # 检查连接是否有效
                    if self._is_connection_valid(conn_info['connection']):
                        conn_info['in_use'] = True
                        return conn_info
                    else:
                        # 重新创建连接
                        new_conn = self._create_connection()
                        if new_conn:
                            conn_info['connection'] = new_conn
                            conn_info['created_at'] = time.time()
                            conn_info['in_use'] = True
                            return conn_info
            
            # 如果没有可用连接，尝试创建新连接
            new_conn = self._create_connection()
            if new_conn:
                conn_info = {
                    'connection': new_conn,
                    'created_at': time.time(),
                    'in_use': True
                }
                self._pool.append(conn_info)
                return conn_info
            
            return None
    
    def _return_connection_to_pool(self, conn_info):
        """归还连接到连接池"""
        with self._pool_lock:
            conn_info['in_use'] = False
    
    def _is_connection_valid(self, conn):
        """检查连接是否有效"""
        try:
            conn.ping(reconnect=True)
            return True
        except:
            return False
    
    def _cleanup_expired_connections(self):
        """清理过期连接"""
        current_time = time.time()
        if current_time - self._last_cleanup < 300:  # 5分钟清理一次
            return
        
        self._last_cleanup = current_time
        expired_connections = []
        
        for i, conn_info in enumerate(self._pool):
            if (not conn_info['in_use'] and 
                current_time - conn_info['created_at'] > self.pool_recycle):
                expired_connections.append(i)
        
        # 移除过期连接
        for i in reversed(expired_connections):
            try:
                self._pool[i]['connection'].close()
            except:
                pass
            del self._pool[i]
        
        if expired_connections:
            logger.info(f"清理了 {len(expired_connections)} 个过期连接")
    
    def execute_query(self, sql: str, params: tuple = None) -> List[Dict]:
        """执行查询语句"""
        with self.get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute(sql, params)
            return cursor.fetchall()
    
    def execute_update(self, sql: str, params: tuple = None) -> int:
        """执行更新语句"""
        with self.get_connection() as conn:
            cursor = conn.cursor()
            affected_rows = cursor.execute(sql, params)
            conn.commit()
            return affected_rows
    
    def execute_insert(self, sql: str, params: tuple = None) -> int:
        """执行插入语句，返回插入的ID"""
        with self.get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute(sql, params)
            conn.commit()
            return cursor.lastrowid
    
    def execute_batch(self, sql: str, params_list: List[tuple]) -> int:
        """批量执行语句"""
        with self.get_connection() as conn:
            cursor = conn.cursor()
            affected_rows = cursor.executemany(sql, params_list)
            conn.commit()
            return affected_rows
    
    def test_connection(self) -> bool:
        """测试数据库连接"""
        try:
            with self.get_connection() as conn:
                cursor = conn.cursor()
                cursor.execute("SELECT 1")
                return True
        except Exception as e:
            logger.error(f"数据库连接测试失败: {e}")
            return False
    
    def close_all_connections(self):
        """关闭所有连接"""
        with self._pool_lock:
            for conn_info in self._pool:
                try:
                    conn_info['connection'].close()
                except:
                    pass
            self._pool.clear()
        logger.info("所有数据库连接已关闭")

# 全局数据库管理器实例
_db_manager = None

def get_db_manager() -> DatabaseManager:
    """获取数据库管理器实例"""
    global _db_manager
    if _db_manager is None:
        _db_manager = DatabaseManager()
    return _db_manager

def init_database_manager(config_file: str = "config.ini"):
    """初始化数据库管理器"""
    global _db_manager
    _db_manager = DatabaseManager(config_file)
    return _db_manager
