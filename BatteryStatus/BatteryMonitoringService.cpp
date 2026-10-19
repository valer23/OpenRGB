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

uint64_t BatteryMonitoringService::Acquire()
{
    std::lock_guard<std::mutex> guard(mutex);
    const auto token = cache.Acquire();
    asus.SetEnabled(true, asus_level_validated);
    return token;
}

void BatteryMonitoringService::Release(uint64_t token)
{
    std::lock_guard<std::mutex> guard(mutex);
    cache.Release(token);
    asus.SetEnabled(cache.Enabled(), asus_level_validated);
}
