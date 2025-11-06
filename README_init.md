# 0. prepare
## step1: install esp-idf
ESP-IDF: Configure ESP-IDF Extension
## step2: gen vsc and docker file
ESP-IDF: Add Docker Container Configuration
ESP-IDF: Add VS Code Configuration Folder

# 1.build bin
## step1: init env
idf-init
## step2: set target
idf-target
## step3: set board type
idf-menu
## step4: build
默认编译：idf-build，但不会读取board的config.json配置文件
针对不同board的编译命令：python3 scripts/release.py atk-dnesp32s3（执行之前记得删除release/*.zip，否则配置仍然不生效）