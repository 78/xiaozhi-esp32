import sys
import os
import json

# 切换到项目根目录
os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def get_board_type():
    """
    从编译命令中提取板子类型 (BOARD_TYPE)。
    返回值: 板子类型字符串，如果未找到则返回 None。
    """
    try:
        # 打开名为 "build/compile_commands.json" 的文件，该文件通常由编译系统生成，包含了项目编译所需的命令信息
        # 使用 with 语句可以确保文件在使用完后自动关闭，避免资源泄漏
        with open("build/compile_commands.json") as f:
            # 使用 json.load 函数将文件内容解析为 Python 数据结构（通常是列表或字典）
            data = json.load(f)
            # 遍历解析后的数据中的每个条目
            for item in data:
                # 检查当前条目中的 "file" 键对应的值是否以 "main.cc" 结尾
                # 目的是筛选出与 main.cc 文件相关的编译命令，因为我们只关心该文件编译时的 BOARD_TYPE 参数
                if not item["file"].endswith("main.cc"):
                    # 如果不满足条件，跳过当前条目，继续处理下一个条目
                    continue
                # 若当前条目是 main.cc 文件的编译命令，提取该条目中 "command" 键对应的值
                # 这个值是完整的编译命令字符串
                command = item["command"]
                # 下面的代码用于从编译命令中提取 -DBOARD_TYPE 参数的值
                # 先使用 split("-DBOARD_TYPE=\\\"") 方法将编译命令按 -DBOARD_TYPE=\\\" 进行分割
                # 分割后会得到一个列表，我们取索引为 1 的元素，即 -DBOARD_TYPE= 后面的部分
                # 接着再使用 split("\\\"") 方法将得到的字符串按 \\\" 进行分割，取索引为 0 的元素
                # 最后使用 strip() 方法去除字符串首尾的空白字符，得到纯净的板子类型名称
                board_type = command.split("-DBOARD_TYPE=\\\"")[1].split("\\\"")[0].strip()
                # 找到所需的板子类型后，直接返回该值
                return board_type
    except (FileNotFoundError, IndexError, KeyError, json.JSONDecodeError):
        # 捕获可能出现的异常
        # FileNotFoundError：如果指定的 "build/compile_commands.json" 文件不存在，会抛出该异常
        # IndexError：如果在使用 split 方法分割字符串时，索引超出了列表范围，会抛出该异常
        # KeyError：如果解析后的 JSON 数据中不包含 "file" 或 "command" 键，会抛出该异常
        # json.JSONDecodeError：如果文件内容不是有效的 JSON 格式，会抛出该异常
        pass
    # 如果遍历完所有条目都没有找到符合条件的编译命令，或者在处理过程中出现异常
    # 则返回 None，表示未找到板子类型
    return None
def get_project_version():
    """
    从 CMakeLists.txt 中提取项目版本号。
    返回值: 项目版本号字符串，如果未找到则返回 None。
    """
    with open("CMakeLists.txt") as f:
        for line in f:
            if line.startswith("set(PROJECT_VER"):
                return line.split("\"")[1].split("\"")[0].strip()
    return None

def merge_bin():
    """
    合并生成的二进制文件。
    如果合并失败，打印错误信息并退出程序。
    """
    if os.system("idf.py merge-bin") != 0:
        print("merge bin failed")
        sys.exit(1)

def zip_bin(board_type, project_version):
    """
    将合并后的二进制文件打包为 ZIP 文件。
    参数:
    - board_type: 板子类型
    - project_version: 项目版本号
    """
    if not os.path.exists("releases"):
        os.makedirs("releases")
    output_path = f"releases/v{project_version}_{board_type}.zip"
    if os.path.exists(output_path):
        os.remove(output_path)
    if os.system(f"zip -j {output_path} build/merged-binary.bin") != 0:
        print("zip bin failed")
        sys.exit(1)
    print(f"zip bin to {output_path} done")

def release_current():
    """
    发布当前配置的板子类型和项目版本。
    """
    merge_bin()
    board_type = get_board_type()
    print("board type:", board_type)
    project_version = get_project_version()
    print("project version:", project_version)
    zip_bin(board_type, project_version)

def get_all_board_types():
    """
    从 CMakeLists.txt 中提取所有板子类型及其配置。
    返回值: 包含板子配置的字典，键为配置名，值为板子类型。
    """
    board_configs = {}
    with open("main/CMakeLists.txt") as f:
        lines = f.readlines()
        for i, line in enumerate(lines):
            # 查找 if(CONFIG_BOARD_TYPE_*) 行
            if "if(CONFIG_BOARD_TYPE_" in line:
                config_name = line.strip().split("if(")[1].split(")")[0]
                # 查找下一行的 set(BOARD_TYPE "xxx")
                next_line = lines[i + 1].strip()
                if next_line.startswith("set(BOARD_TYPE"):
                    board_type = next_line.split('"')[1]
                    board_configs[config_name] = board_type
    return board_configs

def release(board_type, board_config):
    """
    发布指定板子类型的固件。
    参数:
    - board_type: 板子类型
    - board_config: 板子配置名
    """
    config_path = f"main/boards/{board_type}/config.json"
    if not os.path.exists(config_path):
        print(f"跳过 {board_type} 因为 config.json 不存在")
        return

    # 打印项目版本号
    project_version = get_project_version()
    print(f"Project Version: {project_version}")
    release_path = f"releases/v{project_version}_{board_type}.zip"
    if os.path.exists(release_path):
        print(f"跳过 {board_type} 因为 {release_path} 已存在")
        return

    with open(config_path, "r") as f:
        config = json.load(f)
    target = config["target"]
    builds = config["builds"]
    
    for build in builds:
        name = build["name"]
        if not name.startswith(board_type):
            raise ValueError(f"name {name} 必须 {board_type} 开头")

        sdkconfig_append = [f"{board_config}=y"]
        for append in build.get("sdkconfig_append", []):
            sdkconfig_append.append(append)
        print(f"name: {name}")
        print(f"target: {target}")
        for append in sdkconfig_append:
            print(f"sdkconfig_append: {append}")
        # 取消设置 IDF_TARGET 环境变量
        os.environ.pop("IDF_TARGET", None)
        # 调用 set-target
        if os.system(f"idf.py set-target {target}") != 0:
            print("set-target failed")
            sys.exit(1)
        # 追加 sdkconfig 配置
        with open("sdkconfig", "a") as f:
            f.write("\n")
            for append in sdkconfig_append:
                f.write(f"{append}\n")
        # 使用宏 BOARD_NAME 构建
        if os.system(f"idf.py -DBOARD_NAME={name} build") != 0:
            print("build failed")
            sys.exit(1)
        # 合并二进制文件
        if os.system("idf.py merge-bin") != 0:
            print("merge-bin failed")
            sys.exit(1)
        # 打包二进制文件
        zip_bin(name, project_version)
        print("-" * 80)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        # 获取所有板子类型
        board_configs = get_all_board_types()
        found = False
        for board_config, board_type in board_configs.items():
            # 如果命令行参数是 'all' 或匹配当前板子类型，则发布
            if sys.argv[1] == 'all' or board_type == sys.argv[1]:
                release(board_type, board_config)
                found = True
        if not found:
            print(f"未找到板子类型: {sys.argv[1]}")
            print("可用的板子类型:")
            for board_type in board_configs.values():
                print(f"  {board_type}")
    else:
        # 如果没有命令行参数，发布当前配置
        release_current()