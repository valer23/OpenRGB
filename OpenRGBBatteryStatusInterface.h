/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>

// Optional fork capability. Keep OpenRGBPluginAPIInterface and its API-v5 vtable unchanged.
// Host and plugin must use matching C++ runtime, RTTI and JSON library builds.
class OpenRGBBatteryStatusInterface
{
public:
    virtual ~OpenRGBBatteryStatusInterface() = default;
    virtual unsigned int GetBatteryStatusVersion() const = 0;
    virtual uint64_t AcquireBatteryMonitoring() = 0;
    virtual void ReleaseBatteryMonitoring(uint64_t token) = 0;
    virtual nlohmann::json GetBatterySnapshots() = 0;
};

// Separate optional RTTI capability: preserve both the API-v5 and battery-v1
// vtables so existing hosts/plugins can continue using cached snapshots.
class OpenRGBBatteryRefreshInterface
{
public:
    virtual ~OpenRGBBatteryRefreshInterface() = default;
    virtual unsigned int GetBatteryRefreshVersion() const = 0;
    virtual bool RequestAsusBatteryRefresh(uint64_t token, bool owner_confirmed_awake) = 0;
};

// Optional mode selection; existing API-v5, status-v1 and refresh-v1 vtables stay unchanged.
class OpenRGBBatteryModeInterface
{
public:
    virtual ~OpenRGBBatteryModeInterface() = default;
    virtual unsigned int GetBatteryModeVersion() const = 0;
    virtual bool SetAsusBatteryMode(const std::string& mode) = 0;
};
