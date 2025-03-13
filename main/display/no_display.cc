#include "no_display.h"

// NoDisplay类的构造函数
NoDisplay::NoDisplay() {
    // 空实现，无需初始化任何内容
    // 这个类用于模拟一个“无显示”设备，因此不需要实际的硬件初始化
}

// NoDisplay类的析构函数
NoDisplay::~NoDisplay() {
    // 空实现，无需清理任何资源
    // 由于没有实际的硬件资源需要释放，析构函数为空
}

// 锁定函数（模拟锁定操作）
bool NoDisplay::Lock(int timeout_ms) {
    // 直接返回true，表示锁定成功
    // 由于这是一个“无显示”设备，锁定操作没有实际意义，因此总是返回成功
    return true;
}

// 解锁函数（模拟解锁操作）
void NoDisplay::Unlock() {
    // 空实现，无需执行任何操作
    // 由于这是一个“无显示”设备，解锁操作没有实际意义
}