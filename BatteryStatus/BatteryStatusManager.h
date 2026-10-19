/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <array>
#include <nlohmann/json.hpp>

class BatteryStatusManager
{
public:
    using Clock = std::function<uint64_t()>;
    static constexpr const char* CorsairId = "corsair-dark-core-se";
    static constexpr const char* AsusId = "asus-rog-delta-ii";
    static BatteryStatusManager& Get();
    explicit BatteryStatusManager(Clock clock = {});
    uint64_t Acquire();
    void Release(uint64_t token);
    bool Enabled() const { return enabled.load(); }
    void SetCorsairValidated(bool validated) { corsair_validated.store(validated); }
    bool CorsairValidated() const { return corsair_validated.load(); }
    void Publish(const std::string& id, int percentage, int charging, const std::string& availability);
    void SetState(const std::string& id, const std::string& availability, const std::string& detail = "");
    nlohmann::json Snapshots();
private:
    struct Sample
    {
        std::string id, name, availability = "unknown", detail;
        int percentage = -1, charging = -1;
        uint64_t updated = 0;
    };
    Sample* Find(const std::string& id);
    Clock clock;
    std::mutex mutex;
    std::set<uint64_t> subscriptions;
    uint64_t next_token = 0;
    std::atomic<bool> enabled {false}, corsair_validated {true};
    std::array<Sample, 2> samples;
};
