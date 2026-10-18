/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BatteryStatusManager.h"
#include <chrono>

BatteryStatusManager& BatteryStatusManager::Get()
{
    // Constructed before controller registration and destroyed after those controllers.
    static BatteryStatusManager instance;
    return instance;
}

BatteryStatusManager::BatteryStatusManager(Clock clock_value) : clock(std::move(clock_value))
{
    if(!clock)
    {
        clock = [] { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()); };
    }
    samples[0].id = CorsairId;
    samples[0].name = "Corsair Dark Core RGB Pro SE";
    samples[0].detail = "Waiting for native mouse controller and protocol validation";
    samples[1].id = AsusId;
    samples[1].name = "ASUS ROG Delta II";
    samples[1].availability = "unavailable";
    samples[1].detail = "Automatic queries disabled: sleep-safe acquisition not yet validated";
}

uint64_t BatteryStatusManager::Acquire()
{
    std::lock_guard<std::mutex> guard(mutex);
    do { ++next_token; } while(next_token == 0 || subscriptions.count(next_token));
    subscriptions.insert(next_token);
    enabled.store(true);
    return next_token;
}

void BatteryStatusManager::Release(uint64_t token)
{
    std::lock_guard<std::mutex> guard(mutex);
    subscriptions.erase(token);
    enabled.store(!subscriptions.empty());
}

BatteryStatusManager::Sample* BatteryStatusManager::Find(const std::string& id)
{
    for(auto& sample : samples)
    {
        if(sample.id == id) { return &sample; }
    }
    return nullptr;
}

void BatteryStatusManager::Publish(const std::string& id, int percentage, int charging, const std::string& availability)
{
    if(percentage < 0 || percentage > 100) { return; }
    std::lock_guard<std::mutex> guard(mutex);
    auto* sample = Find(id);
    if(!sample) { return; }
    sample->percentage = percentage;
    sample->charging = charging == 0 || charging == 1 ? charging : -1;
    sample->availability = availability;
    sample->detail.clear();
    sample->updated = clock();
}

void BatteryStatusManager::SetState(const std::string& id, const std::string& availability, const std::string& detail)
{
    std::lock_guard<std::mutex> guard(mutex);
    auto* sample = Find(id);
    if(sample) { sample->availability = availability; sample->detail = detail; }
}

nlohmann::json BatteryStatusManager::Snapshots()
{
    std::lock_guard<std::mutex> guard(mutex);
    nlohmann::json result = nlohmann::json::array();
    const auto now = clock();
    for(const auto& sample : samples)
    {
        auto age = now >= sample.updated ? now - sample.updated : 0;
        std::string quality = "unknown";
        if(sample.percentage >= 0)
        {
            quality = sample.availability != "active" ? "last_known" :
                !sample.detail.empty() ? "error" : age > 120000 ? "stale" : "current";
        }
        result.push_back({{"id", sample.id}, {"name", sample.name},
            {"percentage", sample.percentage < 0 ? nlohmann::json(nullptr) : nlohmann::json(sample.percentage)},
            {"charging", sample.charging < 0 ? nlohmann::json(nullptr) : nlohmann::json(sample.charging == 1)},
            {"age_ms", sample.percentage < 0 ? nlohmann::json(nullptr) : nlohmann::json(age)},
            {"availability", sample.availability}, {"quality", quality}, {"detail", sample.detail}});
    }
    return result;
}
