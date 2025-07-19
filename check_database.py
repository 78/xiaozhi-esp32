#!/usr/bin/env python3
"""
检查数据库当前状态
"""
import pymysql
import configparser

def check_database():
    """检查数据库状态"""
    try:
        # 读取配置
        config = configparser.ConfigParser()
        config.read('config.ini', encoding='utf-8')
        
        # 数据库配置
        db_config = {
            'host': config.get('database', 'host'),
            'port': config.getint('database', 'port'),
            'user': config.get('database', 'username'),
            'password': config.get('database', 'password'),
            'database': config.get('database', 'database'),
            'charset': config.get('database', 'charset')
        }
        
        connection = pymysql.connect(**db_config)
        cursor = connection.cursor()
        
        print("检查数据库表...")
        
        # 检查所有表
        cursor.execute("SHOW TABLES")
        tables = cursor.fetchall()
        
        if tables:
            print(f"数据库中的表 ({len(tables)} 个):")
            for table in tables:
                table_name = table[0]
                print(f"\n表: {table_name}")
                
                # 显示表结构
                cursor.execute(f"DESCRIBE {table_name}")
                columns = cursor.fetchall()
                for col in columns:
                    print(f"  {col[0]} - {col[1]} - {col[2]} - {col[3]} - {col[4]} - {col[5]}")
                
                # 显示记录数
                cursor.execute(f"SELECT COUNT(*) FROM {table_name}")
                count = cursor.fetchone()[0]
                print(f"  记录数: {count}")
        else:
            print("数据库中没有表")
        
        cursor.close()
        connection.close()
        
    except Exception as e:
        print(f"检查数据库失败: {e}")

if __name__ == "__main__":
    check_database()
