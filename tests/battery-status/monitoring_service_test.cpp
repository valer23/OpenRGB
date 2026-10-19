#include "BatteryStatus/BatteryMonitoringService.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
int main()
{
    BatteryStatusManager cache;
    std::atomic<int> reads{0};
    auto reader=[&] { ++reads; return nlohmann::json{{"percentage",89},{"charging",nullptr}}; };
    {
        BatteryMonitoringService unvalidated(cache,reader,false,1ms);
        auto token=unvalidated.Acquire();
        assert(cache.Enabled() && reads==0);
        unvalidated.Release(token);
        assert(!cache.Enabled() && reads==0);
    }
    {
        BatteryMonitoringService validated(cache,reader,true,1h);
        const auto first=validated.Acquire(), second=validated.Acquire();
        const auto deadline=std::chrono::steady_clock::now()+2s;
        while(cache.Snapshots()[1]["percentage"].is_null() && std::chrono::steady_clock::now()<deadline) { std::this_thread::sleep_for(1ms); }
        assert(reads==1 && cache.Snapshots()[1]["percentage"]==89);
        validated.SetSuspended(true);
        assert(cache.Enabled() && cache.Snapshots()[1]["availability"]=="suspended");
        assert(cache.Snapshots()[1]["quality"]=="last_known");
        const auto during_suspend=validated.Acquire();
        validated.Release(during_suspend);
        assert(reads==1); // New subscribers cannot restart hardware while suspended.
        validated.SetSuspended(false);
        const auto resume_deadline=std::chrono::steady_clock::now()+2s;
        while(cache.Snapshots()[1]["quality"]!="current" && std::chrono::steady_clock::now()<resume_deadline) { std::this_thread::sleep_for(1ms); }
        assert(reads==2);
        validated.Release(first);
        validated.Release(first); // A duplicate/unknown token cannot stop another subscriber.
        assert(cache.Enabled() && cache.Snapshots()[1]["quality"]=="current");
        validated.Release(second);
        assert(!cache.Enabled() && cache.Snapshots()[1]["quality"]=="last_known");
        assert(reads==2);
    }
    std::cout<<"Core monitoring service: closed hardware gate, shared subscription ownership and final-release join passed\n";
}
