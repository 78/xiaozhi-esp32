#ifndef __BQ27220_H__
#define __BQ27220_H__

#include "../common/i2c_device.h"

// BQ27220 Fuel Gauge Driver
// Reference: Texas Instruments BQ27220 datasheet & esp-brookesia implementation
class Bq27220 : public I2cDevice {
public:
    // Battery Status structure (compatible with BQ27220 register format)
    struct BatteryStatus {
        bool dsg : 1;        // The device is in DISCHARGE
        bool sysdwn : 1;     // System down bit
        bool tda : 1;        // Terminate Discharge Alarm
        bool battpres : 1;   // Battery Present detected
        bool auth_gd : 1;    // Detect inserted battery
        bool ocvgd : 1;      // Good OCV measurement taken
        bool tca : 1;        // Terminate Charge Alarm
        bool rsvd : 1;       // Reserved
        bool chginh : 1;     // Charge inhibit
        bool fc : 1;         // Full-charged is detected
        bool otd : 1;        // Overtemperature in discharge
        bool otc : 1;        // Overtemperature in charge
        bool sleep : 1;      // Device is in SLEEP mode
        bool ocvfail : 1;    // OCV reading failed
        bool ocvcomp : 1;    // OCV measurement complete
        bool fd : 1;         // Full-discharge is detected
    };

    Bq27220(i2c_master_bus_handle_t i2c_bus, uint8_t addr);
    
    // Initialize device and verify device ID
    bool Init();
    
    // Get battery state of charge (0-100%)
    int GetBatteryLevel();
    
    // Get battery voltage in mV
    int GetVoltage();
    
    // Get battery current in mA (positive = charging, negative = discharging)
    int GetCurrent();
    
    // Get battery temperature in Celsius
    int GetTemperature();
    
    // Get remaining capacity in mAh
    int GetRemainingCapacity();
    
    // Get full charge capacity in mAh
    int GetFullCapacity();
    
    // Get design capacity in mAh
    int GetDesignCapacity();
    
    // Get state of health (0-100%)
    int GetStateOfHealth();
    
    // Get battery status flags
    bool GetBatteryStatus(BatteryStatus* status);
    
    // Get firmware version
    uint16_t GetFirmwareVersion();
    
    // Get hardware version
    uint16_t GetHardwareVersion();
    
    // Get average power in mW
    int GetAveragePower();
    
    // Get time to empty in minutes
    int GetTimeToEmpty();
    
    // Get time to full in minutes
    int GetTimeToFull();
    
    // Get cycle count
    int GetCycleCount();
    
    // Check if battery is charging
    bool IsCharging();
    
    // Check if battery is discharging  
    bool IsDischarging();
    
    // Check if battery is fully charged
    bool IsFullyCharged();

    // Configure design capacity (in mAh) - requires full charge cycle to take effect
    bool SetDesignCapacity(uint16_t capacity_mah);
    
    // Reset fuel gauge learning
    bool ResetLearning();
    
private:
    // BQ27220 Standard Commands (from datasheet)
    static constexpr uint8_t CMD_CONTROL = 0x00;
    static constexpr uint8_t CMD_TEMPERATURE = 0x06;
    static constexpr uint8_t CMD_VOLTAGE = 0x08;
    static constexpr uint8_t CMD_BATTERY_STATUS = 0x0A;
    static constexpr uint8_t CMD_CURRENT = 0x0C;
    static constexpr uint8_t CMD_REMAINING_CAPACITY = 0x10;
    static constexpr uint8_t CMD_FULL_CHARGE_CAPACITY = 0x12;
    static constexpr uint8_t CMD_AVERAGE_CURRENT = 0x14;
    static constexpr uint8_t CMD_TIME_TO_EMPTY = 0x16;
    static constexpr uint8_t CMD_TIME_TO_FULL = 0x18;
    static constexpr uint8_t CMD_STANDBY_CURRENT = 0x1A;
    static constexpr uint8_t CMD_MAX_LOAD_CURRENT = 0x1E;
    static constexpr uint8_t CMD_AVERAGE_POWER = 0x24;
    static constexpr uint8_t CMD_CYCLE_COUNT = 0x2A;
    static constexpr uint8_t CMD_STATE_OF_CHARGE = 0x2C;
    static constexpr uint8_t CMD_STATE_OF_HEALTH = 0x2E;
    static constexpr uint8_t CMD_DESIGN_CAPACITY = 0x3C;
    static constexpr uint8_t CMD_MAC_DATA = 0x40;
    
    // Control sub-commands
    static constexpr uint16_t CTRL_DEVICE_NUMBER = 0x0001;
    static constexpr uint16_t CTRL_FW_VERSION = 0x0002;
    static constexpr uint16_t CTRL_HW_VERSION = 0x0003;
    static constexpr uint16_t CTRL_SEAL = 0x0030;
    static constexpr uint16_t CTRL_RESET = 0x0041;
    static constexpr uint16_t CTRL_ENTER_CFG_UPDATE = 0x0090;
    static constexpr uint16_t CTRL_EXIT_CFG_UPDATE_REINIT = 0x0091;
    static constexpr uint16_t CTRL_EXIT_CFG_UPDATE = 0x0092;
    
    // Data Memory addresses (from bq27220_reg.h)
    static constexpr uint16_t DM_FULL_CHARGE_CAPACITY = 0x929D;  // Full Charge Capacity address
    static constexpr uint16_t DM_DESIGN_CAPACITY = 0x929F;  // Design Capacity address
    static constexpr uint16_t DM_DESIGN_ENERGY = 0x92A1;    // Design Energy address
    
    // Unseal keys (default)
    static constexpr uint16_t UNSEAL_KEY1 = 0x0414;
    static constexpr uint16_t UNSEAL_KEY2 = 0x3672;
    
    // Device ID
    static constexpr uint16_t DEVICE_ID = 0x0220;
    
    // Read 16-bit register (little endian)
    uint16_t ReadReg16(uint8_t reg);
    
    // Send control command and read response
    uint16_t ControlCommand(uint16_t sub_cmd);
    
    // Send control command without reading response
    void ControlCommandNoRead(uint16_t sub_cmd);
    
    // Unseal the device for configuration
    bool Unseal();
    
    // Seal the device after configuration
    bool Seal();
    
    // Enter config update mode
    bool EnterConfigUpdate();
    
    // Exit config update mode
    bool ExitConfigUpdate();
    
    // Write data to data memory
    bool WriteDataMemory(uint16_t addr, const uint8_t* data, uint8_t len);
};

#endif // __BQ27220_H__
