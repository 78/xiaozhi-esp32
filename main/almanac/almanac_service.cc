#include "almanac_service.h"

#include "almanac_parsers.h"
#include "http_get.h"
#include "solar_terms.h"

#include <esp_log.h>

#include <cstdio>
#include <exception>

#define TAG "AlmanacService"

namespace {
enum NotifyBits {
    kNotifyKeyUpdated = 1 << 0,
    kNotifyNetwork = 1 << 1,
};
constexpr uint32_t kRetryIntervalMs = 30 * 60 * 1000;
// 聚合数据 老黄历 (docs/api/id/65): /laohuangli/d?date=YYYY-M-D
constexpr const char* kBaseUrl = "https://v.juhe.cn/laohuangli/d";
}  // namespace

AlmanacService& AlmanacService::GetInstance() {
    static AlmanacService instance;
    return instance;
}

void AlmanacService::Start() {
    if (started_.exchange(true)) {
        return;
    }
    BaseType_t ok = xTaskCreate(
        [](void* arg) {
            auto* self = static_cast<AlmanacService*>(arg);
            self->TaskLoop();
            self->task_ = nullptr;
            vTaskDelete(nullptr);
        },
        "almanac_svc", 4096 * 2, this, 4, &task_);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create almanac service task");
        task_ = nullptr;
        started_ = false;
    }
}

void AlmanacService::OnKeyUpdated() {
    if (task_ != nullptr) {
        xTaskNotify(task_, kNotifyKeyUpdated, eSetBits);
    }
}

void AlmanacService::OnNetworkConnected() {
    network_connected_ = true;
    if (task_ != nullptr) {
        xTaskNotify(task_, kNotifyNetwork, eSetBits);
    }
}

void AlmanacService::OnNetworkDisconnected() {
    network_connected_ = false;
    if (task_ != nullptr) {
        xTaskNotify(task_, kNotifyNetwork, eSetBits);
    }
}

AlmanacSnapshot AlmanacService::GetSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void AlmanacService::SetUpdateCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    update_callback_ = std::move(callback);
}

void AlmanacService::Publish(const AlmanacSnapshot& snapshot) {
    std::function<void()> callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = snapshot;
        callback = update_callback_;
    }
    if (callback) {
        callback();
    }
}

void AlmanacService::TaskLoop() {
    auto wait = [](uint32_t ms) {
        uint32_t received = 0;
        xTaskNotifyWait(0, 0xffffffff, &received, pdMS_TO_TICKS(ms));
        return received;
    };

    auto wait_until_tomorrow = [&]() {
        time_t now = time(nullptr);
        struct tm tm_local;
        localtime_r(&now, &tm_local);
        // Seconds to the next local midnight, plus one minute so the new
        // day's data is definitely available server-side.
        uint32_t sleep_s = static_cast<uint32_t>(
            24 * 60 * 60 - (tm_local.tm_hour * 3600 + tm_local.tm_min * 60 +
                            tm_local.tm_sec) + 60);
        wait(sleep_s * 1000);
    };

    bool offline_logged = false;
    bool no_credentials_logged = false;
    AlmanacSnapshot last_unavailable;
    bool have_last_unavailable = false;

    auto signature_same = [](const AlmanacSnapshot& a, const AlmanacSnapshot& b) {
        return a.valid == b.valid && a.no_credentials == b.no_credentials &&
               a.credentials_invalid == b.credentials_invalid &&
               a.lunar_date == b.lunar_date && a.solar_term == b.solar_term &&
               a.yi == b.yi && a.ji == b.ji;
    };
    // no_credentials/credentials_invalid snapshots are only published when the
    // credential state changed, so repeated retries do not retrigger the UI.
    auto publish_unavailable = [&](const AlmanacSnapshot& snapshot) {
        if (have_last_unavailable && signature_same(snapshot, last_unavailable)) {
            return;
        }
        last_unavailable = snapshot;
        have_last_unavailable = true;
        Publish(snapshot);
    };

    for (;;) {
      try {
        if (!network_connected_) {
            if (!offline_logged) {
                ESP_LOGI(TAG, "Offline, waiting for network");
                offline_logged = true;
            }
            wait(kRetryIntervalMs);
            continue;
        }
        offline_logged = false;

        std::string key = key_store_.GetKey();
        if (key.empty()) {
            if (!no_credentials_logged) {
                ESP_LOGI(TAG, "No Juhe key, almanac disabled");
                no_credentials_logged = true;
            }
            AlmanacSnapshot no_credentials;
            no_credentials.no_credentials = true;
            publish_unavailable(no_credentials);
            wait(kRetryIntervalMs);
            continue;
        }
        no_credentials_logged = false;

        time_t now = time(nullptr);
        struct tm tm_local;
        localtime_r(&now, &tm_local);
        // Juhe takes an unpadded date: 2026-9-23.
        char date[40];
        snprintf(date, sizeof(date), "%d-%d-%d", tm_local.tm_year + 1900,
                 tm_local.tm_mon + 1, tm_local.tm_mday);

        std::string url = std::string(kBaseUrl) + "?date=" + date + "&key=" + key;
        auto resp = HttpGet(url, 10000);
        AlmanacData parsed = ParseAlmanacResponse(resp.body, resp.status);
        if (!parsed.ok) {
            if (parsed.credentials_invalid) {
                AlmanacSnapshot invalid;
                invalid.credentials_invalid = true;
                publish_unavailable(invalid);
                ESP_LOGW(TAG, "Almanac key rejected (http %d)", resp.status);
                wait(kRetryIntervalMs);
            } else if (parsed.quota_exceeded) {
                // The free daily quota is spent and resets at midnight, so
                // retrying before tomorrow only burns requests for nothing.
                ESP_LOGW(TAG, "Almanac daily quota exceeded, wait until tomorrow");
                wait_until_tomorrow();
            } else {
                ESP_LOGW(TAG, "Almanac request failed (http %d)", resp.status);
                wait(kRetryIntervalMs);
            }
            continue;
        }

        AlmanacSnapshot snapshot;
        snapshot.valid = true;
        snapshot.lunar_date = parsed.lunar_date;
        // The Juhe almanac has no term field; the term is computed locally.
        snapshot.solar_term = GetSolarTerm(tm_local.tm_year + 1900,
                                           tm_local.tm_mon + 1, tm_local.tm_mday);
        snapshot.yi = parsed.yi;
        snapshot.ji = parsed.ji;
        snapshot.updated_at = time(nullptr);
        Publish(snapshot);
        // Valid data was shown, so the next unavailable state is a real
        // transition even if its fields match an earlier one.
        have_last_unavailable = false;
        std::string term_suffix =
            snapshot.solar_term.empty() ? "" : (" (" + snapshot.solar_term + ")");
        ESP_LOGI(TAG, "Almanac updated: %s%s, yi=%d, ji=%d",
                 snapshot.lunar_date.c_str(), term_suffix.c_str(),
                 static_cast<int>(snapshot.yi.size()), static_cast<int>(snapshot.ji.size()));

        // The almanac changes once per day; sleep until the next midnight.
        wait_until_tomorrow();
      } catch (const std::exception& e) {
          // Never let an exception escape a FreeRTOS task; keep the loop alive.
          ESP_LOGE(TAG, "Task loop error: %s", e.what());
          wait(kRetryIntervalMs);
      } catch (...) {
          ESP_LOGE(TAG, "Task loop unknown error");
          wait(kRetryIntervalMs);
      }
    }
}
