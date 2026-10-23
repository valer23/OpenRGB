/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <nlohmann/json.hpp>

// Exclusive battery-only transport. Automatic level reads require a separate no-wake validation gate.
class AsusDeltaIIBattery
{
public:
    // Change only after owner-firmware evidence establishes no-wake level reads.
    static constexpr bool ReceiverLevelValidated = false;
    static nlohmann::json ReadValidatedReceiverSnapshot(bool sleep_safe_level_query_validated);
    static nlohmann::json InspectAvailability();
    static nlohmann::json Observe();
    static nlohmann::json ReadAwakeSnapshot(bool owner_confirmed_awake);
    static nlohmann::json ReadAuthorizedSleepTest(bool owner_authorized_one_query);
};
