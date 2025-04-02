import sys
import os
import json
import zipfile

# 切换到项目根目录
os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def get_board_type():
    with open("build/compile_commands.json") as f:
        data = json.load(f)
        for item in data:
            if not item["file"].endswith("main.cc"):
                continue
            command = item["command"]
            # extract -DBOARD_TYPE=xxx
            board_type = command.split("-DBOARD_TYPE=\\\"")[1].split("\\\"")[0].strip()
            return board_type
    return None

def get_project_version():
    with open("CMakeLists.txt") as f:
        for line in f:
            if line.startswith("set(PROJECT_VER"):
                return line.split("\"")[1].split("\"")[0].strip()
    return None

def merge_bin():
    if os.system("idf.py merge-bin") != 0:
        print("merge bin failed")
        sys.exit(1)

def zip_bin(board_type, project_version):
    if not os.path.exists("releases"):
        os.makedirs("releases")
    output_path = f"releases/v{project_version}_{board_type}.zip"
    if os.path.exists(output_path):
        os.remove(output_path)
    with zipfile.ZipFile(output_path, 'w', compression=zipfile.ZIP_DEFLATED) as zipf:
        zipf.write("build/merged-binary.bin", arcname="merged-binary.bin")
    print(f"zip bin to {output_path} done")
    

def release_current():
    merge_bin()
    board_type = get_board_type()
    print("board type:", board_type)
    project_version = get_project_version()
    print("project version:", project_version)
    zip_bin(board_type, project_version)

def get_all_board_types():
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
    config_path = f"main/boards/{board_type}/config.json"
    if not os.path.exists(config_path):
        print(f"跳过 {board_type} 因为 config.json 不存在")
        return

    # Print Project Version
    project_version = get_project_version()
    print(f"Project Version: {project_version}", config_path)
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
        # unset IDF_TARGET
        os.environ.pop("IDF_TARGET", None)
        # Call set-target
        if os.system(f"idf.py set-target {target}") != 0:
            print("set-target failed")
            sys.exit(1)
        # Append sdkconfig
        with open("sdkconfig", "a") as f:
            f.write("\n")
            for append in sdkconfig_append:
                f.write(f"{append}\n")
        # Build with macro BOARD_NAME defined to name
        if os.system(f"idf.py -DBOARD_NAME={name} build") != 0:
            print("build failed")
            sys.exit(1)
        # Call merge-bin
        if os.system("idf.py merge-bin") != 0:
            print("merge-bin failed")
            sys.exit(1)
        # Zip bin
        zip_bin(name, project_version)
        print("-" * 80)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        board_configs = get_all_board_types()
        found = False
        for board_config, board_type in board_configs.items():
            if sys.argv[1] == 'all' or board_type == sys.argv[1]:
                release(board_type, board_config)
                found = True
        if not found:
            print(f"未找到板子类型: {sys.argv[1]}")
            print("可用的板子类型:")
            for board_type in board_configs.values():
                print(f"  {board_type}")
    else:
        release_current()
