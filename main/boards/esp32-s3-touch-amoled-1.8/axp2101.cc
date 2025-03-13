#include "axp2101.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>

#define TAG "Axp2101"  // 定义日志标签

// Axp2101类的构造函数
Axp2101::Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
    // ** EFUSE 默认配置 **
    WriteReg(0x22, 0b110); // 启用PWRON > OFFLEVEL作为POWEROFF源
    WriteReg(0x27, 0x10);  // 设置长按4秒关机

    WriteReg(0x93, 0x1C); // 配置ALDO2输出为3.3V

    uint8_t value = ReadReg(0x90); // 读取XPOWERS_AXP2101_LDO_ONOFF_CTRL0寄存器的值
    value = value | 0x02; // 设置第1位（ALDO2）
    WriteReg(0x90, value);  // 启用电源通道

    WriteReg(0x64, 0x03); // 设置充电电压为4.2V
    
    WriteReg(0x61, 0x05); // 设置主电池预充电电流为125mA
    WriteReg(0x62, 0x0A); // 设置主电池充电电流为400mA（0x08-200mA, 0x09-300mA, 0x0A-400mA）
    WriteReg(0x63, 0x15); // 设置主电池终止充电电流为125mA

    WriteReg(0x14, 0x00); // 设置最小系统电压为4.1V（默认4.7V），适用于劣质USB线缆
    WriteReg(0x15, 0x00); // 设置输入电压限制为3.88V，适用于劣质USB线缆
    WriteReg(0x16, 0x05); // 设置输入电流限制为2000mA

    WriteReg(0x24, 0x01); // 设置关机时Vsys阈值为3.2V（默认2.6V，会损坏电池）
    WriteReg(0x50, 0x14); // 设置TS引脚为外部输入（非温度检测）
}

// 获取电池电流方向
int Axp2101::GetBatteryCurrentDirection() {
    return (ReadReg(0x01) & 0b01100000) >> 5;  // 读取寄存器0x01的值并提取电流方向位
}

// 判断是否正在充电
bool Axp2101::IsCharging() {
    return GetBatteryCurrentDirection() == 1;  // 电流方向为1表示正在充电
}

// 判断是否正在放电
bool Axp2101::IsDischarging() {
    return GetBatteryCurrentDirection() == 2;  // 电流方向为2表示正在放电
}

// 判断充电是否完成
bool Axp2101::IsChargingDone() {
    uint8_t value = ReadReg(0x01);  // 读取寄存器0x01的值
    return (value & 0b00000111) == 0b00000100;  // 检查充电状态位
}

// 获取电池电量百分比
int Axp2101::GetBatteryLevel() {
    return ReadReg(0xA4);  // 读取寄存器0xA4的值，返回电池电量百分比
}

// 关机
void Axp2101::PowerOff() {
    uint8_t value = ReadReg(0x10);  // 读取寄存器0x10的值
    value = value | 0x01;  // 设置第0位（关机位）
    WriteReg(0x10, value);  // 写入寄存器0x10，触发关机
}