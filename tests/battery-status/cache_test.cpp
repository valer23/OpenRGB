#include "BatteryStatus/BatteryStatusManager.h"
#include <cassert>
#include <iostream>
int main()
{
    uint64_t now = 1000;
    BatteryStatusManager cache([&] { return now; });
    assert(!cache.Enabled());
    auto a = cache.Acquire(), b = cache.Acquire();
    assert(a != b && cache.Enabled());
    cache.Release(a); cache.Release(a); assert(cache.Enabled());
    cache.Release(b); assert(!cache.Enabled());
    assert(cache.Snapshots().size() == 2);
    cache.Publish(BatteryStatusManager::CorsairId, 76, -1, "active");
    auto row = cache.Snapshots()[0];
    assert(row["percentage"] == 76 && row["charging"].is_null());
    assert(row["age_ms"] == 0 && row["quality"] == "current");
    now += 120001;
    row = cache.Snapshots()[0]; assert(row["quality"] == "stale");
    cache.SetState(BatteryStatusManager::CorsairId, "disconnected", "Receiver removed");
    row = cache.Snapshots()[0];
    assert(row["percentage"] == 76 && row["age_ms"] == 120001);
    assert(row["availability"] == "disconnected" && row["quality"] == "last_known");
    cache.Publish(BatteryStatusManager::CorsairId, 101, 0, "active");
    assert(cache.Snapshots()[0]["percentage"] == 76);
    cache.Publish(BatteryStatusManager::CorsairId, 0, 1, "active");
    assert(cache.Snapshots()[0]["percentage"] == 0);
    cache.SetState(BatteryStatusManager::CorsairId, "active", "Invalid reply");
    row = cache.Snapshots()[0]; assert(row["quality"] == "error");
    cache.Publish(BatteryStatusManager::CorsairId, 42, 7, "active");
    assert(cache.Snapshots()[0]["charging"].is_null());
    cache.Publish("unknown-device", 10, 0, "active");
    assert(cache.Snapshots().size() == 2);
    std::cout << "Cache lifecycle, freshness and invalid-value tests passed\n";
}
