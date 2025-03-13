#include "axp2101.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>

#define TAG "Axp2101"  // 定义日志标签

// AXP2101 电源管理芯片类
Axp2101::Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
    // ** EFUSE 默认配置 **
    WriteReg(0x22, 0b110); // 配置 PWRON > OFFLEVEL 作为 POWEROFF 源使能
    WriteReg(0x27, 0x10);  // 设置长按 4 秒关机

    WriteReg(0x93, 0x1C); // 配置 ALDO2 输出电压为 3.3V

    uint8_t value = ReadReg(0x90); // 读取 LDO 开关控制寄存器 (XPOWERS_AXP2101_LDO_ONOFF_CTRL0)
    value = value | 0x02; // 设置第 1 位 (ALDO2) 为 1
    WriteReg(0x90, value);  // 使能电源通道

    WriteReg(0x64, 0x03); // 设置充电电压为 4.2V
    
    WriteReg(0x61, 0x05); // 设置主电池预充电电流为 125mA
    WriteReg(0x62, 0x0A); // 设置主电池充电电流为 400mA (0x08-200mA, 0x09-300mA, 0x0A-400mA)
    WriteReg(0x63, 0x15); // 设置主电池终止充电电流为 125mA

    WriteReg(0x14, 0x00); // 设置最小系统电压为 4.1V（默认 4.7V），适用于劣质 USB 线缆
    WriteReg(0x15, 0x00); // 设置输入电压限制为 3.88V，适用于劣质 USB 线缆
    WriteReg(0x16, 0x05); // 设置输入电流限制为 2000mA

    WriteReg(0x24, 0x01); // 设置 Vsys 关机阈值为 3.2V（默认 2.6V，避免电池过放）
    WriteReg(0x50, 0x14); // 设置 TS 引脚为外部输入（非温度传感器）
}

// 获取电池电流方向
int Axp2101::GetBatteryCurrentDirection() {
    return (ReadReg(0x01) & 0b01100000) >> 5;  // 读取寄存器 0x01 的第 5-6 位
}

// 判断是否正在充电
bool Axp2101::IsCharging() {
    return GetBatteryCurrentDirection() == 1;  // 电流方向为 1 表示充电
}

// 判断是否正在放电
bool Axp2101::IsDischarging() {
    return GetBatteryCurrentDirection() == 2;  // 电流方向为 2 表示放电
}

// 判断充电是否完成
bool Axp2101::IsChargingDone() {
    uint8_t value = ReadReg(0x01);  // 读取寄存器 0x01
    return (value & 0b00000111) == 0b00000100;  // 检查充电状态位
}

// 获取电池电量百分比
int Axp2101::GetBatteryLevel() {
    return ReadReg(0xA4);  // 读取寄存器 0xA4 获取电池电量
}

// 关机
void Axp2101::PowerOff() {
    uint8_t value = ReadReg(0x10);  // 读取寄存器 0x10
    value = value | 0x01;  // 设置第 0 位为 1
    WriteReg(0x10, value);  // 写入寄存器 0x10 触发关机
}