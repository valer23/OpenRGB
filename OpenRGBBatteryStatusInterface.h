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
