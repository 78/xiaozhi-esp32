#include "network_controller.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <http.h>

#include "settings.h"

namespace {

constexpr int kCellularPowerStabilizeMs = 1'000;
constexpr int kHealthCheckTimeoutMs = 5'000;
constexpr int kLifecycleStopTimeoutMs = 7'000;
constexpr const char* kDefaultHealthCheckUrl = "https://api.tenclass.net/";
constexpr int kProbeConnectId = 3;
constexpr EventBits_t kWorkerStopped = BIT0;
constexpr EventBits_t kWifiProbeStopped = BIT1;
constexpr EventBits_t kCellularProbeStopped = BIT2;
constexpr EventBits_t kAllTasksStopped =
    kWorkerStopped | kWifiProbeStopped | kCellularProbeStopped;
const char* TAG = "NetworkController";

uint64_t NowMs() {
    return static_cast<uint64_t>(esp_timer_get_time() / 1000);
}

}  // namespace

NetworkController::NetworkController(WifiBoard& wifi, Ml307Board& cellular)
    : wifi_(wifi), cellular_(cellular) {
    lifecycle_events_ = xEventGroupCreate();
    if (lifecycle_events_ != nullptr) {
        xEventGroupSetBits(lifecycle_events_, kAllTasksStopped);
    }
    LoadAndMigrateSettings();
}

NetworkController::~NetworkController() {
    Stop();
    if (lifecycle_events_ != nullptr) {
        vEventGroupDelete(lifecycle_events_);
    }
}

void NetworkController::Start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }
    if (lifecycle_events_ == nullptr) {
        running_ = false;
        ESP_LOGE(TAG, "Cannot start without lifecycle event group");
        return;
    }

    StartWifi();

    xEventGroupClearBits(lifecycle_events_, kWorkerStopped);
    if (xTaskCreate(WorkerTaskEntry, "network_ctl", 4096, this, 4, &worker_task_) != pdPASS) {
        worker_task_ = nullptr;
        running_ = false;
        if (lifecycle_events_ != nullptr) {
            xEventGroupSetBits(lifecycle_events_, kWorkerStopped);
        }
        ESP_LOGE(TAG, "Failed to create network controller task");
    }
}

void NetworkController::Stop() {
    running_.exchange(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++wifi_generation_;
        ++cellular_generation_;
        switch_request_pending_ = false;
    }
    wifi_.SetNetworkEventCallback({});
    cellular_.SetNetworkEventCallback({});
    if (lifecycle_events_ != nullptr) {
        const auto bits = xEventGroupWaitBits(lifecycle_events_, kAllTasksStopped, pdFALSE,
                                              pdTRUE,
                                              pdMS_TO_TICKS(kLifecycleStopTimeoutMs));
        if ((bits & kAllTasksStopped) != kAllTasksStopped) {
            ESP_LOGE(TAG, "Timed out waiting for network tasks to stop (bits=0x%lx)",
                     static_cast<unsigned long>(bits));
        }
    }
    if (wifi_started_) {
        wifi_.StopNetwork();
        wifi_started_ = false;
    }
    StopCellularAndPowerOff();
}

bool NetworkController::SetMode(NetworkMode mode) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        policy_.SetMode(mode, NowMs());
        switch_request_pending_ = false;
    }
    SaveMode(mode);
    if (!wifi_started_) {
        StartWifi();
    }
    EvaluatePolicy();
    return true;
}

NetworkMode NetworkController::GetMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return policy_.GetSnapshot().mode;
}

NetworkStatusSnapshot NetworkController::GetStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return policy_.GetSnapshot();
}

NetworkInterface* NetworkController::GetNetwork() const {
    const auto active = GetStatus().active;
    if (active == NetworkTransport::Wifi) {
        return wifi_.GetNetwork();
    }
    if (active == NetworkTransport::Cellular) {
        return cellular_.GetNetwork();
    }
    return nullptr;
}

const char* NetworkController::GetNetworkStateIcon() const {
    return GetStatus().active == NetworkTransport::Cellular ? cellular_.GetNetworkStateIcon()
                                                            : wifi_.GetNetworkStateIcon();
}

void NetworkController::SetPowerSaveLevel(PowerSaveLevel level) {
    wifi_.SetPowerSaveLevel(level);
    cellular_.SetPowerSaveLevel(level);
}

void NetworkController::SetNetworkEventCallback(NetworkEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    network_event_callback_ = std::move(callback);
}

void NetworkController::SetSwitchRequestCallback(SwitchRequestCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    switch_request_callback_ = std::move(callback);
}

void NetworkController::SetCellularPowerControl(std::function<bool(bool)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    cellular_power_control_ = std::move(callback);
}

void NetworkController::SetExternalPowerProvider(std::function<bool()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    external_power_provider_ = std::move(callback);
}

void NetworkController::RefreshPowerPolicy() {
    // The controller worker re-evaluates power policy every second. Keeping this method
    // non-blocking is important because battery notifications may originate on UI tasks.
}

void NetworkController::CommitSwitch(NetworkTransport target, NetworkSwitchReason reason) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        policy_.RecordSwitch(target, reason, NowMs());
        switch_request_pending_ = false;
    }

    Settings settings("network", true);
    settings.SetString("last_good", ToString(target));
    ApplyPowerPolicy();
    NotifyNetworkEvent(NetworkEvent::Connected, ToString(target));
}

void NetworkController::CancelPendingSwitch() {
    std::lock_guard<std::mutex> lock(mutex_);
    switch_request_pending_ = false;
}

void NetworkController::ReportProtocolConnected() {
    const auto active = GetStatus().active;
    if (active != NetworkTransport::None) {
        ReportHealth(active, NetworkHealth::InternetReady);
    }
}

void NetworkController::ReportProtocolFailure() {
    const auto active = GetStatus().active;
    if (active != NetworkTransport::None) {
        ReportHealth(active, NetworkHealth::Degraded);
        ScheduleProbe(active);
    }
}

void NetworkController::WorkerTaskEntry(void* arg) {
    auto* controller = static_cast<NetworkController*>(arg);
    controller->WorkerTask();
    {
        std::lock_guard<std::mutex> lock(controller->mutex_);
        controller->worker_task_ = nullptr;
    }
    if (controller->lifecycle_events_ != nullptr) {
        xEventGroupSetBits(controller->lifecycle_events_, kWorkerStopped);
    }
    vTaskDelete(nullptr);
}

void NetworkController::WorkerTask() {
    while (running_) {
        EvaluatePolicy();

        const auto status = GetStatus();
        if (status.active == NetworkTransport::Cellular) {
            uint64_t next_probe_at = 0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                next_probe_at = policy_.NextWifiProbeAtMs();
            }
            if (NowMs() >= next_probe_at) {
                if (!wifi_started_) {
                    StartWifi();
                }
                ScheduleProbe(NetworkTransport::Wifi);
                std::lock_guard<std::mutex> lock(mutex_);
                policy_.MarkWifiProbeStarted(NowMs());
            }
        }

        ApplyPowerPolicy();
        vTaskDelay(pdMS_TO_TICKS(1'000));
    }
}

void NetworkController::ProbeTaskEntry(void* arg) {
    std::unique_ptr<ProbeContext> context(static_cast<ProbeContext*>(arg));
    auto* controller = context->controller;
    const auto transport = context->transport;
    controller->Probe(transport, context->generation);
    if (controller->lifecycle_events_ != nullptr) {
        xEventGroupSetBits(controller->lifecycle_events_,
                           transport == NetworkTransport::Wifi ? kWifiProbeStopped
                                                               : kCellularProbeStopped);
    }
    vTaskDelete(nullptr);
}

void NetworkController::Probe(NetworkTransport transport, uint32_t generation) {
    NetworkInterface* network =
        transport == NetworkTransport::Wifi ? wifi_.GetNetwork() : cellular_.GetNetwork();
    bool ready = false;
    if (network != nullptr) {
        auto http = network->CreateHttp(kProbeConnectId);
        if (http != nullptr) {
            http->SetTimeout(kHealthCheckTimeoutMs);
            if (http->Open("GET", health_check_url_)) {
                const int status = http->GetStatusCode();
                ready = status >= 200 && status < 500;
            }
            http->Close();
        }
    }

    bool current = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const uint32_t current_generation = transport == NetworkTransport::Wifi
                                                ? wifi_generation_
                                                : cellular_generation_;
        current = NetworkPolicy::IsCurrentGeneration(generation, current_generation);
        if (transport == NetworkTransport::Wifi) {
            wifi_probe_in_progress_ = false;
        } else {
            cellular_probe_in_progress_ = false;
        }
    }
    if (current) {
        ReportHealth(transport,
                     ready ? NetworkHealth::InternetReady : NetworkHealth::Degraded);
    }
}

void NetworkController::StartWifi() {
    if (!running_) {
        return;
    }
    uint32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (wifi_started_) {
            return;
        }
        wifi_started_ = true;
        generation = ++wifi_generation_;
        policy_.ReportHealth(NetworkTransport::Wifi, NetworkHealth::Starting, NowMs());
    }
    wifi_.SetNetworkEventCallback([this, generation](NetworkEvent event, const std::string& data) {
        OnTransportEvent(NetworkTransport::Wifi, generation, event, data);
    });
    wifi_.StartNetwork();
}

void NetworkController::StartCellular() {
    if (!running_) {
        return;
    }
    std::function<bool(bool)> power_callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cellular_started_ || !policy_.CanRetryCellular(NowMs())) {
            return;
        }
        power_callback = cellular_power_control_;
        if (!cellular_powered_) {
            if (power_callback && !power_callback(true)) {
                policy_.ReportHealth(NetworkTransport::Cellular, NetworkHealth::Down, NowMs());
                return;
            }
            cellular_powered_ = true;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(kCellularPowerStabilizeMs));
    if (cellular_.IsNetworkRunning()) {
        return;
    }
    uint32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || cellular_started_ || !policy_.CanRetryCellular(NowMs())) {
            return;
        }
        cellular_started_ = true;
        generation = ++cellular_generation_;
        policy_.ReportHealth(NetworkTransport::Cellular, NetworkHealth::Starting, NowMs());
    }
    cellular_.SetNetworkEventCallback(
        [this, generation](NetworkEvent event, const std::string& data) {
            OnTransportEvent(NetworkTransport::Cellular, generation, event, data);
        });
    cellular_.StartNetwork();
}

void NetworkController::StopCellularAndPowerOff() {
    std::function<bool(bool)> power_callback;
    bool should_stop = false;
    bool was_powered = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cellular_probe_in_progress_) {
            return;
        }
        should_stop = cellular_started_ || cellular_powered_;
        was_powered = cellular_powered_;
        cellular_started_ = false;
        ++cellular_generation_;
        power_callback = cellular_power_control_;
    }
    cellular_.SetNetworkEventCallback({});
    if (should_stop && !cellular_.StopNetwork()) {
        std::lock_guard<std::mutex> lock(mutex_);
        cellular_started_ = true;
        return;
    }
    bool power_disabled = !was_powered;
    if (was_powered && (!power_callback || power_callback(false))) {
        power_disabled = true;
    }
    if (was_powered && power_disabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        cellular_powered_ = false;
    }
    if (should_stop) {
        ReportHealth(NetworkTransport::Cellular, NetworkHealth::Down);
    }
}

void NetworkController::OnTransportEvent(NetworkTransport transport, uint32_t generation,
                                         NetworkEvent event, const std::string& data) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const uint32_t current_generation = transport == NetworkTransport::Wifi
                                                ? wifi_generation_
                                                : cellular_generation_;
        if (!NetworkPolicy::IsCurrentGeneration(generation, current_generation)) {
            ESP_LOGW(TAG, "Ignoring stale %s callback from generation %u",
                     ToString(transport), generation);
            return;
        }
    }

    bool cellular_start_failed = false;
    bool cellular_sim_missing = false;
    switch (event) {
        case NetworkEvent::Scanning:
        case NetworkEvent::Connecting:
        case NetworkEvent::ModemDetecting:
            ReportHealth(transport, NetworkHealth::Starting);
            break;
        case NetworkEvent::Connected:
            ReportHealth(transport, NetworkHealth::LinkUp);
            ScheduleProbe(transport);
            break;
        case NetworkEvent::Disconnected:
        case NetworkEvent::ModemErrorRegDenied:
        case NetworkEvent::ModemErrorInitFailed:
        case NetworkEvent::ModemErrorTimeout:
            if (transport == NetworkTransport::Cellular) {
                cellular_start_failed = true;
            } else {
                ReportHealth(transport, NetworkHealth::Down);
            }
            break;
        case NetworkEvent::ModemErrorNoSim:
            if (transport == NetworkTransport::Cellular) {
                cellular_start_failed = true;
                cellular_sim_missing = true;
            }
            break;
        case NetworkEvent::WifiConfigModeEnter:
        case NetworkEvent::WifiConfigModeExit:
            break;
    }

    if (cellular_start_failed) {
        int failure_count = 0;
        bool retry_limited = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (cellular_sim_missing) {
                policy_.RecordCellularNoSim(NowMs());
            } else {
                policy_.RecordCellularStartFailure(NowMs());
            }
            const auto status = policy_.GetSnapshot();
            failure_count = status.cellular_start_failures;
            retry_limited = status.cellular_retry_limited;
            cellular_started_ = false;
            ++cellular_generation_;
        }
        if (cellular_sim_missing) {
            ESP_LOGW(TAG,
                     "No SIM detected; cellular switching is paused and SIM presence will be "
                     "checked again in 300 seconds");
        } else if (retry_limited) {
            ESP_LOGW(TAG,
                     "Cellular switching inhibited after %d failed starts; waiting 300 seconds "
                     "before the next SIM presence check",
                     failure_count);
        } else {
            ESP_LOGW(TAG, "Cellular start failed (%d/3); SIM presence will be checked again in "
                          "30 seconds",
                     failure_count);
        }
        EvaluatePolicy();
    }

    const auto status = GetStatus();
    const bool preferred_while_starting =
        status.active == NetworkTransport::None &&
        ((status.mode == NetworkMode::Cellular && transport == NetworkTransport::Cellular) ||
         (status.mode != NetworkMode::Cellular && transport == NetworkTransport::Wifi));
    if (event != NetworkEvent::Connected &&
        (transport == status.active || preferred_while_starting)) {
        NotifyNetworkEvent(event, data);
    }
}

void NetworkController::ReportHealth(NetworkTransport transport, NetworkHealth health) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        policy_.ReportHealth(transport, health, NowMs());
    }
    EvaluatePolicy();
}

void NetworkController::EvaluatePolicy() {
    NetworkDecision decision;
    SwitchRequestCallback callback;
    bool initial_switch = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (switch_request_pending_) {
            return;
        }
        decision = policy_.Evaluate(NowMs());
        if (!decision.switch_requested) {
            return;
        }
        initial_switch = policy_.GetSnapshot().active == NetworkTransport::None;
        callback = switch_request_callback_;
        if (!initial_switch) {
            switch_request_pending_ = true;
        }
    }

    if (initial_switch) {
        CommitSwitch(decision.target, decision.reason);
    } else if (callback) {
        callback(decision.target, decision.reason);
    } else {
        std::lock_guard<std::mutex> lock(mutex_);
        switch_request_pending_ = false;
    }
}

void NetworkController::ScheduleProbe(NetworkTransport transport) {
    if (!running_) {
        return;
    }
    uint32_t generation = 0;
    EventBits_t stopped_bit = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool& in_progress = transport == NetworkTransport::Wifi ? wifi_probe_in_progress_
                                                                : cellular_probe_in_progress_;
        if (in_progress) {
            return;
        }
        in_progress = true;
        generation = transport == NetworkTransport::Wifi ? wifi_generation_
                                                          : cellular_generation_;
        stopped_bit = transport == NetworkTransport::Wifi ? kWifiProbeStopped
                                                           : kCellularProbeStopped;
        if (lifecycle_events_ != nullptr) {
            xEventGroupClearBits(lifecycle_events_, stopped_bit);
        }
    }

    auto* context = new ProbeContext{this, transport, generation};
    if (xTaskCreate(ProbeTaskEntry, "network_probe", 4096, context, 3, nullptr) != pdPASS) {
        delete context;
        std::lock_guard<std::mutex> lock(mutex_);
        if (transport == NetworkTransport::Wifi) {
            wifi_probe_in_progress_ = false;
        } else {
            cellular_probe_in_progress_ = false;
        }
        if (lifecycle_events_ != nullptr) {
            xEventGroupSetBits(lifecycle_events_, stopped_bit);
        }
    }
}

void NetworkController::ApplyPowerPolicy() {
    bool external_power = false;
    NetworkMode mode;
    NetworkTransport active;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        external_power = external_power_provider_ ? external_power_provider_() : false;
        const auto status = policy_.GetSnapshot();
        mode = status.mode;
        active = status.active;
    }
    if (NetworkPolicy::ShouldPowerCellular(external_power, mode, active)) {
        StartCellular();
    } else {
        StopCellularAndPowerOff();
    }
}

void NetworkController::NotifyNetworkEvent(NetworkEvent event, const std::string& data) {
    NetworkEventCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = network_event_callback_;
    }
    if (callback) {
        callback(event, data);
    }
}

void NetworkController::LoadAndMigrateSettings() {
    Settings settings("network", true);
    std::string mode_value = settings.GetString("mode");
    NetworkMode mode = NetworkMode::Auto;
    if (!mode_value.empty()) {
        if (!ParseNetworkMode(mode_value, mode)) {
            mode = NetworkMode::Auto;
        }
    } else {
        const int32_t legacy_type = settings.GetInt("type", -1);
        if (legacy_type == 0) {
            mode = NetworkMode::Wifi;
            settings.SetString("last_good", "wifi");
        } else if (legacy_type == 1) {
            mode = NetworkMode::Cellular;
            settings.SetString("last_good", "cellular");
        }
        settings.SetString("mode", ToString(mode));
    }
    health_check_url_ = settings.GetString("health_url", kDefaultHealthCheckUrl);
    policy_.SetMode(mode, NowMs());
}

void NetworkController::SaveMode(NetworkMode mode) {
    Settings settings("network", true);
    settings.SetString("mode", ToString(mode));
}
