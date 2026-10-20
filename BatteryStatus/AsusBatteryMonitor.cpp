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
    {
        std::lock_guard<std::mutex> wait_guard(wait_mutex);
        stop.store(true);
    }
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
            ApplyReply(reply, false);
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

void AsusBatteryMonitor::ApplyReply(const nlohmann::json& reply, bool manual)
{
    const auto level = reply.find("percentage");
    if(level != reply.end() && level->is_number_integer() && *level >= 0 && *level <= 100)
    {
        const auto charging = reply.find("charging");
        const auto charging_error = reply.find("charging_error");
        const std::string detail = charging_error != reply.end() && charging_error->is_string() ?
            charging_error->get<std::string>() : "";
        const bool cached = reply.value("source", nlohmann::json()) == "asus_log";
        const auto age = reply.find("source_age_ms");
        if(cached && (age == reply.end() || !age->is_number_integer() || *age < 0))
        {
            cache.SetState(BatteryStatusManager::AsusId, "unavailable", "Invalid ASUS cache timestamp");
            return;
        }
        const auto state = reply.value("availability", nlohmann::json());
        const std::string availability = cached ? (state == "asus_cache_offline" ? "asus_cache_offline" : "asus_cache") :
            (manual ? "manual" : "receiver");
        const auto cached_detail = reply.find("detail");
        const std::string description = cached && cached_detail != reply.end() && cached_detail->is_string() ?
            cached_detail->get<std::string>() : detail;
        cache.Publish(BatteryStatusManager::AsusId, level->get<int>(),
            charging != reply.end() && charging->is_boolean() ? (charging->get<bool>() ? 1 : 0) : -1,
            availability, description, cached ? age->get<uint64_t>() : 0); // A receiver response does not establish the wireless child's awake state.
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

bool AsusBatteryMonitor::RequestOnce(Reader reader)
{
    std::lock_guard<std::mutex> guard(lifecycle_mutex);
    if(!cache.Enabled() || once_running.load() || (!stop.load() && worker.joinable())) { return false; }
    StopLocked(); // Join a completed request before reusing its thread handle.
    cache.SetState(BatteryStatusManager::AsusId, "refreshing", "One awake-confirmed battery read in progress");
    stop.store(false);
    once_running.store(true);
    try
    {
        worker = std::thread([this, reader = std::move(reader)] {
            try
            {
                const auto reply = reader();
                if(!stop.load() && cache.Enabled()) { ApplyReply(reply, true); }
            }
            catch(const std::exception& error)
            {
                if(!stop.load()) { cache.SetState(BatteryStatusManager::AsusId, "unavailable", error.what()); }
            }
            catch(...)
            {
                if(!stop.load()) { cache.SetState(BatteryStatusManager::AsusId, "unavailable", "Battery provider failed"); }
            }
            stop.store(true);
            once_running.store(false);
        });
    }
    catch(const std::exception& error)
    {
        stop.store(true);
        once_running.store(false);
        cache.SetState(BatteryStatusManager::AsusId, "unavailable", error.what());
        return false;
    }
    return true;
}
