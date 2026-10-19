/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "AsusBatteryMonitor.h"
#include <algorithm>

AsusBatteryMonitor::AsusBatteryMonitor(BatteryStatusManager& cache_value, Reader reader,
    std::chrono::milliseconds interval_value) : cache(cache_value), read(std::move(reader)),
    interval(std::max(std::chrono::milliseconds(1), interval_value))
{
}

AsusBatteryMonitor::~AsusBatteryMonitor()
{
    std::lock_guard<std::mutex> guard(lifecycle_mutex);
    StopLocked();
}

void AsusBatteryMonitor::StopLocked()
{
    stop.store(true);
    changed.notify_all();
    if(worker.joinable())
    {
        worker.join();
        cache.SetState(BatteryStatusManager::AsusId, "unknown", "Monitoring disabled; retained last-known battery");
    }
}

void AsusBatteryMonitor::SetEnabled(bool subscribed, bool no_wake_validated)
{
    std::lock_guard<std::mutex> guard(lifecycle_mutex);
    if(!subscribed || !cache.Enabled() || !no_wake_validated)
    {
        StopLocked();
        if(subscribed && !no_wake_validated)
        {
            cache.SetState(BatteryStatusManager::AsusId, "unavailable", "Automatic queries disabled: sleep-safe acquisition not yet validated");
        }
        return;
    }
    if(worker.joinable()) { return; }
    stop.store(false);
    try { worker = std::thread(&AsusBatteryMonitor::Run, this); }
    catch(const std::exception& error)
    {
        stop.store(true);
        cache.SetState(BatteryStatusManager::AsusId, "unavailable", error.what());
    }
}

void AsusBatteryMonitor::Run()
{
    while(!stop.load() && cache.Enabled())
    {
        try
        {
            const auto reply = read();
            if(stop.load() || !cache.Enabled()) { break; }
            const auto level = reply.find("percentage");
            if(level != reply.end() && level->is_number_integer() && *level >= 0 && *level <= 100)
            {
                const auto charging = reply.find("charging");
                cache.Publish(BatteryStatusManager::AsusId, level->get<int>(),
                    charging != reply.end() && charging->is_boolean() ? (charging->get<bool>() ? 1 : 0) : -1,
                    "receiver"); // A receiver response does not establish the wireless child's awake state.
            }
            else
            {
                const auto error = reply.find("error");
                const auto detail = error != reply.end() && error->is_string() ?
                    error->get<std::string>() : "Invalid battery percentage reply";
                const auto state = reply.find("availability");
                const std::string availability = state != reply.end() && state->is_string() &&
                    (*state == "disconnected" || *state == "unknown") ? state->get<std::string>() : "unavailable";
                cache.SetState(BatteryStatusManager::AsusId, availability, detail);
            }
        }
        catch(const std::exception& error)
        {
            if(!stop.load()) { cache.SetState(BatteryStatusManager::AsusId, "unavailable", error.what()); }
        }
        catch(...)
        {
            if(!stop.load()) { cache.SetState(BatteryStatusManager::AsusId, "unavailable", "Battery provider failed"); }
        }
        std::unique_lock<std::mutex> guard(wait_mutex);
        changed.wait_for(guard, interval, [this] { return stop.load(); });
    }
}
