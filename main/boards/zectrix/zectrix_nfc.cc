#include "zectrix_nfc.h"

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstring>
#include <utility>

extern "C" void __attribute__((weak)) BoardI2cForcePowerOn() {}

namespace {

constexpr char kTag[] = "ZectrixNfc";
constexpr int kI2cTimeoutMs = 100;
constexpr uint32_t kPowerOnDelayUs = 1000;
constexpr uint32_t kPowerCycleDelayUs = 5000;
constexpr uint32_t kReadDelayUs = 10000;
constexpr uint32_t kTransferDelayUs = 5000;
constexpr TickType_t kFieldDebounceDelay = pdMS_TO_TICKS(20);
constexpr uint8_t kType2NdefTlv = 0x03;
constexpr uint8_t kType2TerminatorTlv = 0xFE;
constexpr uint8_t kType2LongLengthMarker = 0xFF;

std::vector<uint8_t> BuildTextNdefMessage(const std::string& text, const std::string& language) {
    std::vector<uint8_t> payload;
    payload.reserve(1 + language.size() + text.size());
    payload.push_back(static_cast<uint8_t>(language.size() & 0x3F));
    payload.insert(payload.end(), language.begin(), language.end());
    payload.insert(payload.end(), text.begin(), text.end());

    std::vector<uint8_t> message;
    if (payload.size() <= 0xFF) {
        message.reserve(4 + payload.size());
        message.push_back(0xD1);
        message.push_back(0x01);
        message.push_back(static_cast<uint8_t>(payload.size()));
    } else {
        message.reserve(7 + payload.size());
        message.push_back(0xC1);
        message.push_back(0x01);
        const uint32_t payload_len = static_cast<uint32_t>(payload.size());
        message.push_back(static_cast<uint8_t>((payload_len >> 24) & 0xFF));
        message.push_back(static_cast<uint8_t>((payload_len >> 16) & 0xFF));
        message.push_back(static_cast<uint8_t>((payload_len >> 8) & 0xFF));
        message.push_back(static_cast<uint8_t>(payload_len & 0xFF));
    }
    message.push_back('T');
    message.insert(message.end(), payload.begin(), payload.end());
    return message;
}

std::vector<uint8_t> BuildUriNdefMessage(const std::string& uri) {
    uint8_t prefix_code = 0x00;
    std::string uri_suffix = uri;
    if (uri.rfind("https://www.", 0) == 0) {
        prefix_code = 0x02;
        uri_suffix = uri.substr(strlen("https://www."));
    } else if (uri.rfind("http://www.", 0) == 0) {
        prefix_code = 0x01;
        uri_suffix = uri.substr(strlen("http://www."));
    } else if (uri.rfind("https://", 0) == 0) {
        prefix_code = 0x04;
        uri_suffix = uri.substr(strlen("https://"));
    } else if (uri.rfind("http://", 0) == 0) {
        prefix_code = 0x03;
        uri_suffix = uri.substr(strlen("http://"));
    }

    std::vector<uint8_t> payload;
    payload.reserve(1 + uri_suffix.size());
    payload.push_back(prefix_code);
    payload.insert(payload.end(), uri_suffix.begin(), uri_suffix.end());

    std::vector<uint8_t> message;
    if (payload.size() <= 0xFF) {
        message.reserve(4 + payload.size());
        message.push_back(0xD1);
        message.push_back(0x01);
        message.push_back(static_cast<uint8_t>(payload.size()));
    } else {
        message.reserve(7 + payload.size());
        message.push_back(0xC1);
        message.push_back(0x01);
        const uint32_t payload_len = static_cast<uint32_t>(payload.size());
        message.push_back(static_cast<uint8_t>((payload_len >> 24) & 0xFF));
        message.push_back(static_cast<uint8_t>((payload_len >> 16) & 0xFF));
        message.push_back(static_cast<uint8_t>((payload_len >> 8) & 0xFF));
        message.push_back(static_cast<uint8_t>(payload_len & 0xFF));
    }
    message.push_back('U');
    message.insert(message.end(), payload.begin(), payload.end());
    return message;
}

std::vector<uint8_t> BuildType2NdefTlv(const std::vector<uint8_t>& message) {
    std::vector<uint8_t> tlv;
    if (message.size() <= 0xFE) {
        tlv.reserve(2 + message.size() + 1);
        tlv.push_back(kType2NdefTlv);
        tlv.push_back(static_cast<uint8_t>(message.size()));
    } else {
        tlv.reserve(4 + message.size() + 1);
        tlv.push_back(kType2NdefTlv);
        tlv.push_back(kType2LongLengthMarker);
        tlv.push_back(static_cast<uint8_t>((message.size() >> 8) & 0xFF));
        tlv.push_back(static_cast<uint8_t>(message.size() & 0xFF));
    }
    tlv.insert(tlv.end(), message.begin(), message.end());
    tlv.push_back(kType2TerminatorTlv);
    return tlv;
}

}  // namespace

ZectrixNfc::ZectrixNfc(i2c_master_bus_handle_t i2c_bus,
                       uint8_t addr,
                       gpio_num_t power_gpio,
                       gpio_num_t fd_gpio,
                       int fd_active_level)
    : I2cDevice(i2c_bus, addr),
      power_gpio_(power_gpio),
      fd_gpio_(fd_gpio),
      fd_active_level_(fd_active_level) {}

bool ZectrixNfc::Init() {
    bool expected = false;
    if (!initialized_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return true;
    }

    gpio_config_t power_cfg = {};
    power_cfg.pin_bit_mask = 1ULL << power_gpio_;
    power_cfg.mode = GPIO_MODE_OUTPUT;
    power_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    power_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    power_cfg.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&power_cfg) != ESP_OK) {
        ESP_LOGE(kTag, "failed to config NFC power gpio=%d", power_gpio_);
        initialized_.store(false, std::memory_order_release);
        return false;
    }
    PowerOff();

    gpio_config_t fd_cfg = {};
    fd_cfg.pin_bit_mask = 1ULL << fd_gpio_;
    fd_cfg.mode = GPIO_MODE_INPUT;
    fd_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    fd_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    fd_cfg.intr_type = GPIO_INTR_ANYEDGE;
    if (gpio_config(&fd_cfg) != ESP_OK) {
        ESP_LOGE(kTag, "failed to config NFC FD gpio=%d", fd_gpio_);
        initialized_.store(false, std::memory_order_release);
        return false;
    }

    if (EnsureIsrServiceInstalled() != ESP_OK) {
        initialized_.store(false, std::memory_order_release);
        return false;
    }

    BaseType_t task_ok =
        xTaskCreate(&ZectrixNfc::FieldTaskEntry, "zectrix_nfc_fd", 3 * 1024, this, 3, &field_task_);
    if (task_ok != pdPASS || field_task_ == nullptr) {
        ESP_LOGE(kTag, "failed to create NFC field task");
        initialized_.store(false, std::memory_order_release);
        return false;
    }

    esp_err_t isr_ret = gpio_isr_handler_add(fd_gpio_, &ZectrixNfc::FieldIsrHandler, this);
    if (isr_ret != ESP_OK) {
        ESP_LOGE(kTag, "failed to install NFC FD ISR: %s", esp_err_to_name(isr_ret));
        initialized_.store(false, std::memory_order_release);
        return false;
    }

    if (!PowerOn()) {
        initialized_.store(false, std::memory_order_release);
        return false;
    }

    std::array<uint8_t, 7> uid = {};
    const esp_err_t uid_ret = ReadUid(&uid);
    UpdateFieldState(IsFieldLevelActive(), false);

    ESP_LOGI(kTag,
             "NFC init: addr=0x%02X power_gpio=%d fd_gpio=%d fd_active=%d capacity=%u field=%d uid_ret=%s uid=%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             static_cast<unsigned>(device_address_),
             power_gpio_,
             fd_gpio_,
             fd_active_level_,
             static_cast<unsigned>(kUserDataCapacity),
             HasField() ? 1 : 0,
             esp_err_to_name(uid_ret),
             uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6]);
    return true;
}

bool ZectrixNfc::PowerOn() {
    ScopedI2cBusLock bus_lock("ZectrixNfc::PowerOn");
    if (!bus_lock.locked()) {
        ESP_LOGW(kTag, "NFC power on bus lock failed: %s", esp_err_to_name(bus_lock.status()));
        powered_.store(false, std::memory_order_release);
        return false;
    }
    gpio_hold_dis(power_gpio_);
    gpio_set_level(power_gpio_, 1);
    gpio_hold_en(power_gpio_);
    esp_rom_delay_us(kPowerOnDelayUs);
    BoardI2cForcePowerOn();
    const esp_err_t ret = Probe();
    if (ret != ESP_OK) {
        ESP_LOGW(kTag, "NFC probe failed after power on: %s", esp_err_to_name(ret));
        powered_.store(false, std::memory_order_release);
        return false;
    }
    powered_.store(true, std::memory_order_release);
    UpdateFieldState(IsFieldLevelActive(), false);
    return true;
}

void ZectrixNfc::PowerOff() {
    powered_.store(false, std::memory_order_release);
    gpio_hold_dis(power_gpio_);
    gpio_set_level(power_gpio_, 0);
    gpio_hold_en(power_gpio_);
    UpdateFieldState(false, true);
}

bool ZectrixNfc::IsPowered() const {
    return powered_.load(std::memory_order_acquire);
}

bool ZectrixNfc::HasField() const {
    return field_present_.load(std::memory_order_acquire);
}

void ZectrixNfc::SetFieldCallback(std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    field_callback_ = std::move(callback);
}

esp_err_t ZectrixNfc::ReadBlock(uint8_t block_addr, uint8_t out[kBlockSize]) {
    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t ret = EnsureReadyForTransferLocked();
    if (ret != ESP_OK) return ret;
    ret = BeginTransferSessionLocked("read_block");
    if (ret != ESP_OK) return ret;
    ret = ReadBlockInternal(block_addr, out);
    EndTransferSessionLocked();
    return ret;
}

esp_err_t ZectrixNfc::WriteBlock(uint8_t block_addr, const uint8_t data[kBlockSize]) {
    if (data == nullptr) return ESP_ERR_INVALID_ARG;
    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t ret = EnsureReadyForTransferLocked();
    if (ret != ESP_OK) return ret;
    ret = BeginTransferSessionLocked("write_block");
    if (ret != ESP_OK) return ret;
    ret = WriteBlockInternal(block_addr, data, kBlockSize);
    EndTransferSessionLocked();
    return ret;
}

esp_err_t ZectrixNfc::ReadUserData(uint16_t offset, uint8_t* out, size_t len) {
    if (out == nullptr && len != 0) return ESP_ERR_INVALID_ARG;
    if (len == 0) return ESP_OK;
    if (!IsValidUserRange(offset, len)) return ESP_ERR_INVALID_SIZE;

    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t ret = EnsureReadyForTransferLocked();
    if (ret != ESP_OK) return ret;
    ret = BeginTransferSessionLocked("read_user_data");
    if (ret != ESP_OK) return ret;

    size_t remaining = len;
    size_t dst_offset = 0;
    uint16_t current_offset = offset;
    while (remaining > 0) {
        const uint16_t block_index = current_offset / kBlockSize;
        const uint8_t block_addr = static_cast<uint8_t>(kUserDataStartBlock + block_index);
        const size_t in_block_offset = current_offset % kBlockSize;
        const size_t chunk = std::min(remaining, kBlockSize - in_block_offset);
        uint8_t block[kBlockSize] = {};
        ret = ReadBlockInternal(block_addr, block);
        if (ret != ESP_OK) { EndTransferSessionLocked(); return ret; }
        memcpy(out + dst_offset, block + in_block_offset, chunk);
        current_offset += static_cast<uint16_t>(chunk);
        dst_offset += chunk;
        remaining -= chunk;
    }
    EndTransferSessionLocked();
    return ESP_OK;
}

esp_err_t ZectrixNfc::WriteUserData(uint16_t offset, const uint8_t* data, size_t len) {
    if ((data == nullptr && len != 0) || (offset % kBlockSize) != 0) return ESP_ERR_INVALID_ARG;
    if (len == 0) return ESP_OK;
    if (!IsValidUserRange(offset, len)) return ESP_ERR_INVALID_SIZE;

    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t ret = EnsureReadyForTransferLocked();
    if (ret != ESP_OK) return ret;
    ret = BeginTransferSessionLocked("write_user_data");
    if (ret != ESP_OK) return ret;

    size_t remaining = len;
    size_t src_offset = 0;
    uint16_t current_offset = offset;
    while (remaining > 0) {
        const uint8_t block_addr =
            static_cast<uint8_t>(kUserDataStartBlock + (current_offset / kBlockSize));
        const size_t chunk = std::min(remaining, kBlockSize);
        ret = WriteBlockInternal(block_addr, data + src_offset, chunk);
        if (ret != ESP_OK) { EndTransferSessionLocked(); return ret; }
        current_offset += static_cast<uint16_t>(chunk);
        src_offset += chunk;
        remaining -= chunk;
    }
    EndTransferSessionLocked();
    return ESP_OK;
}

esp_err_t ZectrixNfc::ClearUserData(uint16_t offset, size_t len) {
    if ((offset % kBlockSize) != 0) return ESP_ERR_INVALID_ARG;
    if (len == 0) return ESP_OK;
    if (!IsValidUserRange(offset, len)) return ESP_ERR_INVALID_SIZE;

    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t ret = EnsureReadyForTransferLocked();
    if (ret != ESP_OK) return ret;
    ret = BeginTransferSessionLocked("clear_user_data");
    if (ret != ESP_OK) return ret;

    std::array<uint8_t, kBlockSize> zeros = {};
    size_t remaining = len;
    uint16_t current_offset = offset;
    while (remaining > 0) {
        const uint8_t block_addr =
            static_cast<uint8_t>(kUserDataStartBlock + (current_offset / kBlockSize));
        const size_t chunk = std::min(remaining, kBlockSize);
        ret = WriteBlockInternal(block_addr, zeros.data(), chunk);
        if (ret != ESP_OK) { EndTransferSessionLocked(); return ret; }
        current_offset += static_cast<uint16_t>(chunk);
        remaining -= chunk;
    }
    EndTransferSessionLocked();
    return ESP_OK;
}

esp_err_t ZectrixNfc::ReadCommandArea(uint8_t* out, size_t len) {
    if (out == nullptr && len != 0) return ESP_ERR_INVALID_ARG;
    if (len == 0) return ESP_OK;
    if (!IsValidCommandRange(len)) return ESP_ERR_INVALID_SIZE;

    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t ret = EnsureReadyForTransferLocked();
    if (ret != ESP_OK) return ret;
    ret = BeginTransferSessionLocked("read_command_area");
    if (ret != ESP_OK) return ret;

    size_t remaining = len;
    size_t dst_offset = 0;
    uint8_t block_addr = kCommandBlockAddress;
    while (remaining > 0) {
        uint8_t block[kBlockSize] = {};
        ret = ReadBlockInternal(block_addr, block);
        if (ret != ESP_OK) { EndTransferSessionLocked(); return ret; }
        const size_t chunk = std::min(remaining, kBlockSize);
        memcpy(out + dst_offset, block, chunk);
        dst_offset += chunk;
        remaining -= chunk;
        ++block_addr;
    }
    EndTransferSessionLocked();
    return ESP_OK;
}

esp_err_t ZectrixNfc::ClearCommandArea(size_t len) {
    if (len == 0) return ESP_OK;
    if (!IsValidCommandRange(len)) return ESP_ERR_INVALID_SIZE;

    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t ret = EnsureReadyForTransferLocked();
    if (ret != ESP_OK) return ret;
    ret = BeginTransferSessionLocked("clear_command_area");
    if (ret != ESP_OK) return ret;

    std::array<uint8_t, kBlockSize> zeros = {};
    size_t remaining = len;
    uint8_t block_addr = kCommandBlockAddress;
    while (remaining > 0) {
        const size_t chunk = std::min(remaining, kBlockSize);
        ret = WriteBlockInternal(block_addr, zeros.data(), chunk);
        if (ret != ESP_OK) { EndTransferSessionLocked(); return ret; }
        remaining -= chunk;
        ++block_addr;
    }
    EndTransferSessionLocked();
    return ESP_OK;
}

esp_err_t ZectrixNfc::ReadUid(std::array<uint8_t, 7>* uid) {
    if (uid == nullptr) return ESP_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t ret = EnsureReadyForTransferLocked();
    if (ret != ESP_OK) return ret;
    ret = BeginTransferSessionLocked("read_uid");
    if (ret != ESP_OK) return ret;

    uint8_t block[kBlockSize] = {};
    ret = ReadBlockInternal(kUidBlockAddress, block);
    EndTransferSessionLocked();
    if (ret != ESP_OK) return ret;
    std::copy(block, block + uid->size(), uid->begin());
    return ESP_OK;
}

size_t ZectrixNfc::GetUserDataCapacity() const {
    return kUserDataCapacity;
}

esp_err_t ZectrixNfc::ReadNdef(std::vector<uint8_t>* message) {
    if (message == nullptr) return ESP_ERR_INVALID_ARG;

    std::vector<uint8_t> raw(kUserDataCapacity, 0);
    esp_err_t ret = ReadUserData(0, raw.data(), raw.size());
    if (ret != ESP_OK) return ret;

    size_t cursor = 0;
    while (cursor < raw.size()) {
        const uint8_t tlv_type = raw[cursor++];
        if (tlv_type == 0x00) continue;
        if (tlv_type == kType2TerminatorTlv) return ESP_ERR_NOT_FOUND;
        if (cursor >= raw.size()) return ESP_ERR_INVALID_RESPONSE;

        size_t length = raw[cursor++];
        if (length == kType2LongLengthMarker) {
            if ((cursor + 1) >= raw.size()) return ESP_ERR_INVALID_RESPONSE;
            length = (static_cast<size_t>(raw[cursor]) << 8) |
                     static_cast<size_t>(raw[cursor + 1]);
            cursor += 2;
        }

        if ((cursor + length) > raw.size()) return ESP_ERR_INVALID_RESPONSE;

        if (tlv_type == kType2NdefTlv) {
            if (length == 0) return ESP_ERR_NOT_FOUND;
            message->assign(raw.begin() + static_cast<std::ptrdiff_t>(cursor),
                            raw.begin() + static_cast<std::ptrdiff_t>(cursor + length));
            return ESP_OK;
        }

        cursor += length;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t ZectrixNfc::WriteNdef(const std::vector<uint8_t>& message) {
    if (message.empty()) return ESP_ERR_INVALID_ARG;
    const size_t tlv_header_size = message.size() <= 0xFE ? 2 : 4;
    if (message.size() > (kUserDataCapacity - tlv_header_size - 1)) return ESP_ERR_INVALID_SIZE;

    const std::vector<uint8_t> tlv = BuildType2NdefTlv(message);
    return WriteUserData(0, tlv.data(), tlv.size());
}

esp_err_t ZectrixNfc::WriteTextNdef(const std::string& text, const std::string& language) {
    return WriteNdef(BuildTextNdefMessage(text, language));
}

esp_err_t ZectrixNfc::WriteUriNdef(const std::string& uri) {
    return WriteNdef(BuildUriNdefMessage(uri));
}

void ZectrixNfc::FieldTaskEntry(void* arg) {
    auto* self = static_cast<ZectrixNfc*>(arg);
    if (self == nullptr) { vTaskDelete(nullptr); return; }
    self->FieldTask();
}

void ZectrixNfc::FieldIsrHandler(void* arg) {
    auto* self = static_cast<ZectrixNfc*>(arg);
    if (self == nullptr || self->field_task_ == nullptr) return;
    BaseType_t high_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(self->field_task_, &high_task_woken);
    if (high_task_woken == pdTRUE) portYIELD_FROM_ISR();
}

esp_err_t ZectrixNfc::EnsureIsrServiceInstalled() {
    static std::mutex isr_mutex;
    std::lock_guard<std::mutex> lock(isr_mutex);
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret == ESP_ERR_INVALID_STATE) return ESP_OK;
    return ret;
}

esp_err_t ZectrixNfc::Probe() {
    ScopedI2cBusLock bus_lock("ZectrixNfc::Probe");
    if (!bus_lock.locked()) return bus_lock.status();
    BoardI2cForcePowerOn();
    return i2c_master_probe(i2c_bus_, device_address_, kI2cTimeoutMs);
}

esp_err_t ZectrixNfc::BeginTransferSessionLocked(const char* reason) {
    i2c_session_lock_ = std::make_unique<ScopedI2cBusLock>("ZectrixNfc::TransferSession");
    if (!i2c_session_lock_->locked()) {
        esp_err_t ret = i2c_session_lock_->status();
        i2c_session_lock_.reset();
        return ret;
    }
    gpio_hold_dis(power_gpio_);
    gpio_set_level(power_gpio_, 0);
    esp_rom_delay_us(kPowerCycleDelayUs);
    gpio_set_level(power_gpio_, 1);
    gpio_hold_en(power_gpio_);
    BoardI2cForcePowerOn();
    esp_rom_delay_us(kPowerOnDelayUs);

    esp_err_t ret = Probe();
    if (ret != ESP_OK) {
        ESP_LOGW(kTag, "transfer session probe failed: reason=%s ret=%s",
                 reason ? reason : "unknown", esp_err_to_name(ret));
        i2c_session_lock_.reset();
    }
    return ret;
}

void ZectrixNfc::EndTransferSessionLocked() {
    esp_rom_delay_us(kTransferDelayUs);
    UpdateFieldState(IsPowered() && IsFieldLevelActive(), false);
    i2c_session_lock_.reset();
}

esp_err_t ZectrixNfc::ExecuteTransferLocked(const char* reason,
                                            const std::function<esp_err_t()>& op) {
    esp_err_t ret = op();
    if (ret == ESP_ERR_INVALID_STATE || ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(kTag, "i2c transfer retry: reason=%s ret=%s",
                 reason ? reason : "unknown", esp_err_to_name(ret));
        if (ResetBus(reason) == ESP_OK) {
            BoardI2cForcePowerOn();
            ret = op();
        }
    }
    return ret;
}

esp_err_t ZectrixNfc::ReadBlockInternal(uint8_t block_addr, uint8_t out[kBlockSize]) {
    if (out == nullptr) return ESP_ERR_INVALID_ARG;
    if (block_addr > kUserDataEndBlock) return ESP_ERR_INVALID_ARG;

    const uint8_t addr = block_addr;
    esp_err_t ret = ExecuteTransferLocked("nfc_read_addr", [&]() -> esp_err_t {
        BoardI2cForcePowerOn();
        return i2c_master_transmit(i2c_device_, &addr, sizeof(addr), kI2cTimeoutMs);
    });
    if (ret != ESP_OK) return ret;

    esp_rom_delay_us(kReadDelayUs);
    ret = ExecuteTransferLocked("nfc_read_data", [&]() -> esp_err_t {
        BoardI2cForcePowerOn();
        return i2c_master_receive(i2c_device_, out, kBlockSize, kI2cTimeoutMs);
    });
    esp_rom_delay_us(kTransferDelayUs);
    return ret;
}

esp_err_t ZectrixNfc::WriteBlockInternal(uint8_t block_addr, const uint8_t* data, size_t len) {
    if (data == nullptr || len > kBlockSize) return ESP_ERR_INVALID_ARG;
    if (block_addr > kUserDataEndBlock) return ESP_ERR_INVALID_ARG;

    uint8_t buffer[kBlockSize + 1] = {};
    buffer[0] = block_addr;
    memcpy(buffer + 1, data, len);
    esp_err_t ret = ExecuteTransferLocked("nfc_write_block", [&]() -> esp_err_t {
        BoardI2cForcePowerOn();
        return i2c_master_transmit(i2c_device_, buffer, sizeof(buffer), kI2cTimeoutMs);
    });
    esp_rom_delay_us(kTransferDelayUs);
    return ret;
}

esp_err_t ZectrixNfc::EnsureReadyForTransferLocked() {
    if (!initialized_.load(std::memory_order_acquire)) return ESP_ERR_INVALID_STATE;
    if (!powered_.load(std::memory_order_acquire)) return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}

bool ZectrixNfc::IsValidUserRange(uint16_t offset, size_t len) const {
    if (offset > kUserDataCapacity) return false;
    return len <= (kUserDataCapacity - offset);
}

bool ZectrixNfc::IsValidCommandRange(size_t len) const {
    return len <= kCommandAreaCapacity;
}

bool ZectrixNfc::IsFieldLevelActive() const {
    return gpio_get_level(fd_gpio_) == fd_active_level_;
}

void ZectrixNfc::UpdateFieldState(bool field_present, bool invoke_callback) {
    const bool previous = field_present_.exchange(field_present, std::memory_order_acq_rel);
    if (!invoke_callback || previous == field_present) return;
    DispatchFieldState(field_present);
}

void ZectrixNfc::DispatchFieldState(bool field_present) {
    std::function<void(bool)> callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = field_callback_;
    }
    if (callback) callback(field_present);
}

void ZectrixNfc::FieldTask() {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(kFieldDebounceDelay);
        const bool field_present = IsPowered() && IsFieldLevelActive();
        UpdateFieldState(field_present, true);
    }
}
