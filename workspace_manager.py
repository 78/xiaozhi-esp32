#!/usr/bin/env python3
import os
import shutil
import logging
import time
from pathlib import Path
from datetime import datetime
import json

logger = logging.getLogger("shaolu.workspace")

class WorkspaceManager:
    """工作目录管理器，为每个设备创建独立的工作环境"""
    
    def __init__(self, base_path=None):
        self.base_path = base_path or os.getcwd()
        self.instances_dir = os.path.join(self.base_path, "instances")
        self.template_dir = os.path.join(self.base_path, "templates", "project_template")
        
        # 确保目录存在
        os.makedirs(self.instances_dir, exist_ok=True)
        os.makedirs(os.path.dirname(self.template_dir), exist_ok=True)
        
        logger.info(f"工作目录管理器初始化完成")
        logger.info(f"基础路径: {self.base_path}")
        logger.info(f"实例目录: {self.instances_dir}")
    
    def create_project_template(self):
        """创建项目模板，复制必要的源文件"""
        try:
            if os.path.exists(self.template_dir):
                logger.info("项目模板已存在，跳过创建")
                return True
                
            logger.info("正在创建项目模板...")
            
            # 需要复制的文件和目录
            essential_items = [
                "main",
                "CMakeLists.txt", 
                "sdkconfig.defaults",
                "sdkconfig.defaults.esp32c3",
                "sdkconfig.defaults.esp32s3",
                "partitions.csv",
                "partitions_4M.csv", 
                "partitions_8M.csv",
                "partitions_32M_sensecap.csv",
                "managed_components",
                "_internal"
            ]
            
            os.makedirs(self.template_dir, exist_ok=True)
            
            for item in essential_items:
                src_path = os.path.join(self.base_path, item)
                dst_path = os.path.join(self.template_dir, item)
                
                if os.path.exists(src_path):
                    if os.path.isdir(src_path):
                        if os.path.exists(dst_path):
                            shutil.rmtree(dst_path)
                        shutil.copytree(src_path, dst_path)
                        logger.debug(f"复制目录: {item}")
                    else:
                        shutil.copy2(src_path, dst_path)
                        logger.debug(f"复制文件: {item}")
                else:
                    logger.warning(f"源文件不存在，跳过: {item}")
            
            logger.info("项目模板创建完成")
            return True
            
        except Exception as e:
            logger.error(f"创建项目模板失败: {e}")
            return False
    
    def create_device_workspace(self, device_id, port=None):
        """为设备创建独立的工作目录"""
        try:
            # 生成唯一的工作目录名
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            if port:
                # 先处理端口名称中的特殊字符
                safe_port = port.replace('/', '_').replace('\\', '_')
                workspace_name = f"device_{safe_port}_{timestamp}"
            else:
                workspace_name = f"device_{device_id}_{timestamp}"
            
            workspace_path = os.path.join(self.instances_dir, workspace_name)
            
            # 确保项目模板存在
            if not self.create_project_template():
                return None
                
            logger.info(f"正在为设备 {device_id} 创建工作目录: {workspace_path}")
            
            # 复制项目模板到工作目录
            if os.path.exists(workspace_path):
                shutil.rmtree(workspace_path)
                
            shutil.copytree(self.template_dir, workspace_path)
            
            # 创建设备信息文件
            device_info = {
                "device_id": device_id,
                "port": port,
                "created_at": datetime.now().isoformat(),
                "workspace_name": workspace_name
            }
            
            info_file = os.path.join(workspace_path, "device_info.json")
            with open(info_file, "w", encoding="utf-8") as f:
                json.dump(device_info, f, ensure_ascii=False, indent=4)
            
            logger.info(f"设备工作目录创建成功: {workspace_path}")
            return workspace_path
            
        except Exception as e:
            logger.error(f"创建设备工作目录失败: {e}")
            return None
    
    def cleanup_workspace(self, workspace_path):
        """清理工作目录"""
        try:
            if os.path.exists(workspace_path) and os.path.isdir(workspace_path):
                shutil.rmtree(workspace_path)
                logger.info(f"工作目录已清理: {workspace_path}")
                return True
            return False
        except Exception as e:
            logger.error(f"清理工作目录失败: {e}")
            return False
    
    def list_workspaces(self):
        """列出所有工作目录"""
        try:
            workspaces = []
            if not os.path.exists(self.instances_dir):
                return workspaces
                
            for item in os.listdir(self.instances_dir):
                item_path = os.path.join(self.instances_dir, item)
                if os.path.isdir(item_path):
                    info_file = os.path.join(item_path, "device_info.json")
                    if os.path.exists(info_file):
                        try:
                            with open(info_file, "r", encoding="utf-8") as f:
                                device_info = json.load(f)
                            device_info["workspace_path"] = item_path
                            workspaces.append(device_info)
                        except Exception as e:
                            logger.warning(f"读取设备信息失败: {info_file}, {e}")
                            
            return workspaces
        except Exception as e:
            logger.error(f"列出工作目录失败: {e}")
            return []
    
    def cleanup_old_workspaces(self, max_age_hours=24):
        """清理超过指定时间的旧工作目录"""
        try:
            cleaned_count = 0
            current_time = datetime.now()
            
            for workspace_info in self.list_workspaces():
                try:
                    created_at = datetime.fromisoformat(workspace_info["created_at"])
                    age_hours = (current_time - created_at).total_seconds() / 3600
                    
                    if age_hours > max_age_hours:
                        if self.cleanup_workspace(workspace_info["workspace_path"]):
                            cleaned_count += 1
                            logger.info(f"清理旧工作目录: {workspace_info['workspace_name']}")
                            
                except Exception as e:
                    logger.warning(f"处理工作目录时出错: {e}")
                    
            logger.info(f"清理完成，共清理 {cleaned_count} 个旧工作目录")
            return cleaned_count
            
        except Exception as e:
            logger.error(f"清理旧工作目录失败: {e}")
            return 0
    
    def get_workspace_info(self, workspace_path):
        """获取工作目录信息"""
        try:
            info_file = os.path.join(workspace_path, "device_info.json")
            if os.path.exists(info_file):
                with open(info_file, "r", encoding="utf-8") as f:
                    return json.load(f)
            return None
        except Exception as e:
            logger.error(f"获取工作目录信息失败: {e}")
            return None
    
    def update_workspace_info(self, workspace_path, updates):
        """更新工作目录信息"""
        try:
            info_file = os.path.join(workspace_path, "device_info.json")
            info = self.get_workspace_info(workspace_path) or {}
            info.update(updates)
            info["updated_at"] = datetime.now().isoformat()
            
            with open(info_file, "w", encoding="utf-8") as f:
                json.dump(info, f, ensure_ascii=False, indent=4)
            return True
        except Exception as e:
            logger.error(f"更新工作目录信息失败: {e}")
            return False 