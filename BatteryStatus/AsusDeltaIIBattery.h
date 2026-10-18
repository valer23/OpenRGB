/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <nlohmann/json.hpp>

// Bounded owner-hardware validation only. No automatic polling is enabled here.
class AsusDeltaIIBattery
{
public:
    static nlohmann::json Observe();
    static nlohmann::json ReadAwakeSnapshot(bool owner_confirmed_awake);
};
