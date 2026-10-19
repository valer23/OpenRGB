/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BatteryMonitoringService.h"
#include "AsusDeltaIIBattery.h"

BatteryMonitoringService& BatteryMonitoringService::Get()
{
    static BatteryMonitoringService instance(BatteryStatusManager::Get(), [] {
        return AsusDeltaIIBattery::ReadValidatedReceiverSnapshot(true);
    }, AsusDeltaIIBattery::ReceiverLevelValidated);
    return instance;
}

BatteryMonitoringService::BatteryMonitoringService(BatteryStatusManager& cache_value,
    AsusBatteryMonitor::Reader reader, bool validated, std::chrono::milliseconds interval) :
    cache(cache_value), asus(cache_value, std::move(reader), interval), asus_level_validated(validated)
{
}

void BatteryMonitoringService::UpdateLocked()
{
    asus.SetEnabled(cache.Enabled() && !suspended, asus_level_validated);
    if(suspended)
    {
        cache.SetState(BatteryStatusManager::AsusId, "suspended", "System suspended; retained last-known battery");
    }
}

uint64_t BatteryMonitoringService::Acquire()
{
    std::lock_guard<std::mutex> guard(mutex);
    const auto token = cache.Acquire();
    UpdateLocked();
    return token;
}

void BatteryMonitoringService::Release(uint64_t token)
{
    std::lock_guard<std::mutex> guard(mutex);
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
