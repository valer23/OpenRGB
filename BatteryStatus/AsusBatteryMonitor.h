/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "BatteryStatusManager.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

// Core-owned scheduling only. The injected reader owns and serializes its transport.
class AsusBatteryMonitor
{
public:
    using Reader = std::function<nlohmann::json()>;
    AsusBatteryMonitor(BatteryStatusManager& cache, Reader reader,
        std::chrono::milliseconds interval = std::chrono::seconds(60));
    ~AsusBatteryMonitor();
    void SetEnabled(bool subscribed, bool no_wake_validated);
    bool RequestOnce(Reader reader);
private:
    void StopLocked();
    void Run();
    void ApplyReply(const nlohmann::json& reply, bool manual);
    BatteryStatusManager& cache;
    Reader read;
    const std::chrono::milliseconds interval;
    std::mutex lifecycle_mutex, wait_mutex;
    std::condition_variable changed;
    std::atomic<bool> stop{true}, once_running{false};
    std::thread worker;
};
