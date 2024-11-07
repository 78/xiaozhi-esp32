import sys
import os
import json


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
    if os.system(f"zip -j {output_path} build/merged-binary.bin") != 0:
        print("zip bin failed")
        sys.exit(1)
    print(f"zip bin to {output_path} done")
    

if __name__ == "__main__":
    merge_bin()
    board_type = get_board_type()
    print("board type:", board_type)
    project_version = get_project_version()
    print("project version:", project_version)
    zip_bin(board_type, project_version)
