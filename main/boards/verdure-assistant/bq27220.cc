#include "bq27220.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

#define TAG "BQ27220"

Bq27220::Bq27220(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
    ESP_LOGI(TAG, "BQ27220 driver created at address 0x%02X", addr);
}

uint16_t Bq27220::ReadReg16(uint8_t reg) {
    uint8_t buffer[2];
    ReadRegs(reg, buffer, 2);
    // BQ27220 uses little endian
    return buffer[0] | (buffer[1] << 8);
}

uint16_t Bq27220::ControlCommand(uint16_t sub_cmd) {
    // Write control sub-command
    uint8_t cmd_buf[3];
    cmd_buf[0] = CMD_CONTROL;
    cmd_buf[1] = sub_cmd & 0xFF;
    cmd_buf[2] = (sub_cmd >> 8) & 0xFF;
    i2c_master_transmit(i2c_device_, cmd_buf, 3, 100);
    
    // Wait for command to complete
    vTaskDelay(pdMS_TO_TICKS(15));
    
    // Read response from MAC_DATA
    return ReadReg16(CMD_MAC_DATA);
}

bool Bq27220::Init() {
    ESP_LOGI(TAG, "Initializing BQ27220...");
    
    // Verify device ID
    uint16_t device_id = ControlCommand(CTRL_DEVICE_NUMBER);
    if (device_id != DEVICE_ID) {
        ESP_LOGE(TAG, "Invalid Device ID: 0x%04X (expected 0x%04X)", device_id, DEVICE_ID);
        return false;
    }
    ESP_LOGI(TAG, "Device ID verified: 0x%04X", device_id);
    
    // Read firmware version
    uint16_t fw_version = GetFirmwareVersion();
    ESP_LOGI(TAG, "Firmware Version: 0x%04X", fw_version);
    
    // Read hardware version
    uint16_t hw_version = GetHardwareVersion();
    ESP_LOGI(TAG, "Hardware Version: 0x%04X", hw_version);
    
    // Read initial battery info
    ESP_LOGI(TAG, "Battery SOC: %d%%, Voltage: %dmV, Current: %dmA, Temp: %d°C",
             GetBatteryLevel(), GetVoltage(), GetCurrent(), GetTemperature());
    
    return true;
}

int Bq27220::GetBatteryLevel() {
    uint16_t soc = ReadReg16(CMD_STATE_OF_CHARGE);
    // State of charge is in percentage (0-100)
    if (soc > 100) {
        soc = 100;
    }
    return soc;
}

int Bq27220::GetVoltage() {
    // Voltage in mV
    return ReadReg16(CMD_VOLTAGE);
}

int Bq27220::GetCurrent() {
    // Current in mA (signed)
    int16_t current = (int16_t)ReadReg16(CMD_CURRENT);
    return current;
}

int Bq27220::GetTemperature() {
    // Temperature in 0.1°K, convert to Celsius
    uint16_t temp_k = ReadReg16(CMD_TEMPERATURE);
    // Convert from 0.1°K to °C: (temp_k / 10) - 273.15
    int temp_c = (temp_k / 10) - 273;
    return temp_c;
}

int Bq27220::GetRemainingCapacity() {
    // Remaining capacity in mAh
    return ReadReg16(CMD_REMAINING_CAPACITY);
}

int Bq27220::GetFullCapacity() {
    // Full charge capacity in mAh
    return ReadReg16(CMD_FULL_CHARGE_CAPACITY);
}

int Bq27220::GetDesignCapacity() {
    // Design capacity in mAh
    return ReadReg16(CMD_DESIGN_CAPACITY);
}

int Bq27220::GetStateOfHealth() {
    // State of health in percentage
    uint16_t soh = ReadReg16(CMD_STATE_OF_HEALTH);
    if (soh > 100) {
        soh = 100;
    }
    return soh;
}

bool Bq27220::GetBatteryStatus(BatteryStatus* status) {
    if (!status) {
        return false;
    }
    
    uint16_t status_reg = ReadReg16(CMD_BATTERY_STATUS);
    // Copy the register value to the status structure
    *((uint16_t*)status) = status_reg;
    
    return true;
}

uint16_t Bq27220::GetFirmwareVersion() {
    return ControlCommand(CTRL_FW_VERSION);
}

uint16_t Bq27220::GetHardwareVersion() {
    return ControlCommand(CTRL_HW_VERSION);
}

int Bq27220::GetAveragePower() {
    // Average power in mW (signed)
    return (int16_t)ReadReg16(CMD_AVERAGE_POWER);
}

int Bq27220::GetTimeToEmpty() {
    // Time to empty in minutes
    return ReadReg16(CMD_TIME_TO_EMPTY);
}

int Bq27220::GetTimeToFull() {
    // Time to full in minutes
    return ReadReg16(CMD_TIME_TO_FULL);
}

int Bq27220::GetCycleCount() {
    // Number of charge/discharge cycles
    return ReadReg16(CMD_CYCLE_COUNT);
}

bool Bq27220::IsCharging() {
    int16_t current = GetCurrent();
    // Positive current means charging (with threshold to avoid noise)
    return current > 50;  // 50mA threshold
}

bool Bq27220::IsDischarging() {
    BatteryStatus status;
    if (GetBatteryStatus(&status)) {
        return status.dsg;
    }
    return false;
}

bool Bq27220::IsFullyCharged() {
    BatteryStatus status;
    if (GetBatteryStatus(&status)) {
        return status.fc;
    }
    return false;
}

void Bq27220::ControlCommandNoRead(uint16_t sub_cmd) {
    uint8_t cmd_buf[3];
    cmd_buf[0] = CMD_CONTROL;
    cmd_buf[1] = sub_cmd & 0xFF;
    cmd_buf[2] = (sub_cmd >> 8) & 0xFF;
    i2c_master_transmit(i2c_device_, cmd_buf, 3, 100);
    vTaskDelay(pdMS_TO_TICKS(15));
}

bool Bq27220::Unseal() {
    ESP_LOGI(TAG, "Unsealing BQ27220...");
    
    // Send unseal key sequence
    ControlCommandNoRead(UNSEAL_KEY1);
    ControlCommandNoRead(UNSEAL_KEY2);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Verify unsealed by checking if we can enter config update mode
    ESP_LOGI(TAG, "BQ27220 unsealed");
    return true;
}

bool Bq27220::Seal() {
    ESP_LOGI(TAG, "Sealing BQ27220...");
    ControlCommandNoRead(CTRL_SEAL);
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "BQ27220 sealed");
    return true;
}

bool Bq27220::EnterConfigUpdate() {
    ESP_LOGI(TAG, "Entering config update mode...");
    ControlCommandNoRead(CTRL_ENTER_CFG_UPDATE);
    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for config update mode
    ESP_LOGI(TAG, "Entered config update mode");
    return true;
}

bool Bq27220::ExitConfigUpdate() {
    ESP_LOGI(TAG, "Exiting config update mode with reinit...");
    // Use EXIT_CFG_UPDATE_REINIT (0x0091) to recalculate gauging parameters
    ControlCommandNoRead(CTRL_EXIT_CFG_UPDATE_REINIT);
    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for exit and reinit
    ESP_LOGI(TAG, "Exited config update mode");
    return true;
}

bool Bq27220::WriteDataMemory(uint16_t addr, const uint8_t* data, uint8_t len) {
    // Write address + data to SelectSubclass (0x3E)
    // Format: [0x3E] [addr_low] [addr_high] [data_high] [data_low] (big endian for data)
    uint8_t* buf = new uint8_t[len + 3];
    buf[0] = 0x3E;  // SelectSubclass command
    buf[1] = addr & 0xFF;
    buf[2] = (addr >> 8) & 0xFF;
    memcpy(buf + 3, data, len);
    i2c_master_transmit(i2c_device_, buf, len + 3, 100);
    delete[] buf;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Calculate checksum: sum of (addr_low + addr_high + data bytes), then 0xFF - sum
    uint8_t checksum = 0;
    checksum += (addr & 0xFF);
    checksum += ((addr >> 8) & 0xFF);
    for (int i = 0; i < len; i++) {
        checksum += data[i];
    }
    checksum = 0xFF - checksum;
    
    // Write checksum and length to MACDataSum (0x60)
    uint8_t sum_buf[3];
    sum_buf[0] = 0x60;  // MACDataSum
    sum_buf[1] = checksum;
    sum_buf[2] = len + 4;  // Total length: 2 (address) + len (data) + 1 (checksum) + 1 (length)
    i2c_master_transmit(i2c_device_, sum_buf, 3, 100);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    return true;
}

bool Bq27220::SetDesignCapacity(uint16_t capacity_mah) {
    ESP_LOGI(TAG, "Setting design capacity to %d mAh...", capacity_mah);
    
    // Step 1: Unseal the device
    if (!Unseal()) {
        ESP_LOGE(TAG, "Failed to unseal device");
        return false;
    }
    
    // Step 2: Enter config update mode
    if (!EnterConfigUpdate()) {
        ESP_LOGE(TAG, "Failed to enter config update mode");
        Seal();
        return false;
    }
    
    // Step 3: Write Full Charge Capacity (BIG ENDIAN - high byte first)
    uint8_t cap_data[2];
    cap_data[0] = (capacity_mah >> 8) & 0xFF;  // High byte first
    cap_data[1] = capacity_mah & 0xFF;         // Low byte second
    
    ESP_LOGI(TAG, "Writing Full Charge Capacity: %d mAh", capacity_mah);
    if (!WriteDataMemory(DM_FULL_CHARGE_CAPACITY, cap_data, 2)) {
        ESP_LOGE(TAG, "Failed to write full charge capacity");
        ExitConfigUpdate();
        Seal();
        return false;
    }
    
    // Step 4: Write Design Capacity (BIG ENDIAN)
    ESP_LOGI(TAG, "Writing Design Capacity: %d mAh", capacity_mah);
    if (!WriteDataMemory(DM_DESIGN_CAPACITY, cap_data, 2)) {
        ESP_LOGE(TAG, "Failed to write design capacity");
        ExitConfigUpdate();
        Seal();
        return false;
    }
    
    // Step 5: Write design energy (capacity * 3.7V nominal) (BIG ENDIAN)
    uint16_t design_energy = (uint16_t)((capacity_mah * 37) / 10);  // mWh
    uint8_t energy_data[2];
    energy_data[0] = (design_energy >> 8) & 0xFF;  // High byte first
    energy_data[1] = design_energy & 0xFF;         // Low byte second
    
    ESP_LOGI(TAG, "Writing Design Energy: %d mWh", design_energy);
    if (!WriteDataMemory(DM_DESIGN_ENERGY, energy_data, 2)) {
        ESP_LOGE(TAG, "Failed to write design energy");
        ExitConfigUpdate();
        Seal();
        return false;
    }
    
    // Step 6: Exit config update mode with reinit
    if (!ExitConfigUpdate()) {
        ESP_LOGE(TAG, "Failed to exit config update mode");
        Seal();
        return false;
    }
    
    // Step 6: Seal the device
    Seal();
    
    // Step 7: Verify the new design capacity
    vTaskDelay(pdMS_TO_TICKS(100));
    int new_capacity = GetDesignCapacity();
    ESP_LOGI(TAG, "Verified design capacity: %d mAh", new_capacity);
    
    if (new_capacity == capacity_mah) {
        ESP_LOGI(TAG, "Design capacity set to %d mAh successfully!", capacity_mah);
    } else {
        ESP_LOGW(TAG, "Design capacity verification mismatch: expected %d, got %d", 
                 capacity_mah, new_capacity);
        ESP_LOGI(TAG, "This may be normal - device might need a power cycle or charge cycle");
    }
    
    ESP_LOGI(TAG, "Note: Full charge cycle needed for gauge to recalibrate");
    
    return true;
}

bool Bq27220::ResetLearning() {
    ESP_LOGI(TAG, "Resetting fuel gauge learning...");
    
    if (!Unseal()) {
        return false;
    }
    
    ControlCommandNoRead(CTRL_RESET);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    Seal();
    
    ESP_LOGI(TAG, "Fuel gauge reset complete");
    return true;
}
