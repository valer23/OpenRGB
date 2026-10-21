/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BatteryMonitoringService.h"
#include "AsusBatteryLog.h"
#include "AsusDeltaIIBattery.h"

BatteryMonitoringService& BatteryMonitoringService::Get()
{
    static BatteryMonitoringService instance(BatteryStatusManager::Get(), [] {
        return AsusBatteryLog::Read();
    }, false, std::chrono::seconds(5), [] { return AsusDeltaIIBattery::ReadAwakeSnapshot(true); }, true);
    return instance;
}

BatteryMonitoringService::BatteryMonitoringService(BatteryStatusManager& cache_value,
    AsusBatteryMonitor::Reader reader, bool validated, std::chrono::milliseconds interval, AsusBatteryMonitor::Reader awake, bool cached) :
    cache(cache_value), asus(cache_value, std::move(reader), interval), asus_level_validated(validated), passive_cache(cached), awake_reader(std::move(awake))
{
    cache.SetAsusCacheMode(passive_cache);
}

void BatteryMonitoringService::UpdateLocked()
{
    if(passive_cache || asus_level_validated || suspended || !cache.Enabled())
    {
        asus.SetEnabled(cache.Enabled() && !suspended, passive_cache || asus_level_validated);
    }
    if(suspended)
    {
        cache.SetState(BatteryStatusManager::AsusId, "suspended", "System suspended; retained last-known battery");
    }
}

uint64_t BatteryMonitoringService::Acquire()
{
    std::lock_guard<std::mutex> guard(mutex);
    const auto token = cache.Acquire();
    subscriptions.insert(token);
    UpdateLocked();
    return token;
}

void BatteryMonitoringService::Release(uint64_t token)
{
    std::lock_guard<std::mutex> guard(mutex);
    subscriptions.erase(token);
    cache.Release(token);
    UpdateLocked();
}

void BatteryMonitoringService::SetSuspended(bool value)
{
    std::lock_guard<std::mutex> guard(mutex);
    if(suspended == value) { return; }
    suspended = value;
    if(!suspended && cache.Enabled() && asus_level_validated)
    {
        // Every native read freshly enumerates, opens exclusively and validates
        // identity/descriptor before its request, including after reconnect.
        cache.SetState(BatteryStatusManager::AsusId, "unknown", "Revalidating receiver after system resume");
    }
    UpdateLocked();
}

bool BatteryMonitoringService::SetAsusMode(const std::string& mode)
{
    if(mode!="cache" && mode!="direct") { return false; }
    std::lock_guard<std::mutex> guard(mutex);
    const bool cached=mode=="cache";
    if(!cached && !awake_reader) { return false; }
    if(passive_cache==cached) { return true; }
    // Join old work and discard late results before changing the source label.
    asus.SetEnabled(false,false);
    passive_cache=cached;
    cache.SetAsusCacheMode(cached);
    UpdateLocked();
    return true;
}

bool BatteryMonitoringService::RequestAsusRefresh(uint64_t token, bool owner_confirmed_awake)
{
    std::lock_guard<std::mutex> guard(mutex);
    // The optional manual path never weakens automatic acquisition validation.
    if(passive_cache || !owner_confirmed_awake || suspended || !subscriptions.count(token) ||
       asus_level_validated || !awake_reader) { return false; }
    return asus.RequestOnce(awake_reader);
}
