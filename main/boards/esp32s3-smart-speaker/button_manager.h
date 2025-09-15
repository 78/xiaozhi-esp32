#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "button.h"
#include "config.h"

class ButtonManager {
public:
    static ButtonManager& GetInstance();
    
    // 初始化按钮系统
    bool Initialize();
    
    // 检查是否已初始化
    bool IsInitialized() const { return initialized_; }

private:
    ButtonManager();
    ~ButtonManager() = default;
    ButtonManager(const ButtonManager&) = delete;
    ButtonManager& operator=(const ButtonManager&) = delete;
    
    void SetupButtonCallbacks();
    
    bool initialized_ = false;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    Button test_button_;
};

#endif // BUTTON_MANAGER_H
