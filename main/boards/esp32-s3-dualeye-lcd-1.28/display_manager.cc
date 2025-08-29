#include "display_manager.h"
#include "esp_log.h"

std::vector<Display*> DisplayManager::displays_;
Display* DisplayManager::primary_display_ = nullptr;

void DisplayManager::AddDisplay(Display* display, bool is_primary) {
    if (!display) return;
    
    displays_.push_back(display);
    if (is_primary || displays_.size() == 1) {
        primary_display_ = display;
    }
    ESP_LOGI("DisplayManager", "Display added, total: %d", displays_.size());
}

void DisplayManager::RemoveDisplay(Display* display) {
    auto it = std::find(displays_.begin(), displays_.end(), display);
    if (it != displays_.end()) {
        displays_.erase(it);
        if (primary_display_ == display) {
            primary_display_ = displays_.empty() ? nullptr : displays_[0];
        }
    }
}

size_t DisplayManager::GetDisplayCount() {
    return displays_.size();
}

Display* DisplayManager::GetPrimaryDisplay() {
    return primary_display_;
}

const std::vector<Display*>& DisplayManager::GetAllDisplays() {
    return displays_;
}

// 应用到所有屏幕
void DisplayManager::SetEmotion(const char* emotion) {
    for (auto display : displays_) {
        if (display) display->SetEmotion(emotion);
    }
}

void DisplayManager::SetIcon(const char* icon) {
    for (auto display : displays_) {
        if (display) display->SetIcon(icon);
    }
}

void DisplayManager::SetPreviewImage(const lv_img_dsc_t* img_dsc) {
    for (auto display : displays_) {
        if (display) display->SetPreviewImage(img_dsc);
    }
}

void DisplayManager::SetChatMessage(const char* role, const char* content) {
    for (auto display : displays_) {
        if (display) display->SetChatMessage(role, content);
    }
}

void DisplayManager::SetTheme(const std::string& theme_name) {
    for (auto display : displays_) {
        if (display) display->SetTheme(theme_name);
    }
}

void DisplayManager::SetStatus(const char* status) {
    for (auto display : displays_) {
        if (display) display->SetStatus(status);
    }
}
void DisplayManager::ShowNotification(const char* message, int duration_ms) {
    for (auto display : displays_) {
        if (display) {
            display->ShowNotification(message, duration_ms);
        }
    }
}

void DisplayManager::ShowNotification(const std::string& notification, int duration_ms) {
    for (auto display : displays_) {
        if (display) {
            display->ShowNotification(notification, duration_ms);
        }
    }
}

void DisplayManager::UpdateStatusBar(bool update_all) {
    for (auto display : displays_) {
        if (display) {
            display->UpdateStatusBar(update_all);
        }
    }
}

std::string DisplayManager::GetTheme() {
    if (primary_display_) {
        return primary_display_->GetTheme();
    }
    return displays_.empty() ? "" : displays_[0]->GetTheme();
}

bool DisplayManager::Lock(int timeout_ms) {
    return true;
}

void DisplayManager::Unlock() {
}