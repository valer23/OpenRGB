/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "AsusBatteryMonitor.h"

// Core lifetime, independent of plugin/DLL objects. Serializes subscriptions and
// joins the one ASUS worker when the final subscriber releases monitoring.
class BatteryMonitoringService
{
public:
    static BatteryMonitoringService& Get();
    BatteryMonitoringService(BatteryStatusManager& cache, AsusBatteryMonitor::Reader reader,
        bool asus_level_validated, std::chrono::milliseconds interval = std::chrono::seconds(60),
        AsusBatteryMonitor::Reader awake_reader = {}, bool passive_cache = false);
    uint64_t Acquire();
    void Release(uint64_t token);
    void SetSuspended(bool value);
    bool SetAsusMode(const std::string& mode);
    bool RequestAsusRefresh(uint64_t token, bool owner_confirmed_awake);
private:
    void UpdateLocked();
    bool suspended = false;
    BatteryStatusManager& cache;
    AsusBatteryMonitor asus;
    const bool asus_level_validated;
    bool passive_cache;
    AsusBatteryMonitor::Reader awake_reader;
    std::set<uint64_t> subscriptions;
    std::mutex mutex;
};
