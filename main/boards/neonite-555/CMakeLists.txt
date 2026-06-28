# NEONITE-555 板级文件编译单元
# 通过 add_subdirectory() 被 main/CMakeLists.txt 调用
# 收集所有 .c / .cc / .cpp 文件并追加到父级 SRCS 列表

file(GLOB_RECURSE BOARD_LOCAL_SRCS
    "*.c"
    "*.cc"
    "*.cpp"
)

list(APPEND SRCS ${BOARD_LOCAL_SRCS})
set(SRCS ${SRCS} PARENT_SCOPE)

# 头文件路径
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR})
set(INCLUDE_DIRS ${INCLUDE_DIRS} PARENT_SCOPE)
