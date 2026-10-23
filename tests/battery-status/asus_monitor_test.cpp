#include "BatteryStatus/AsusBatteryMonitor.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;

template<class Predicate> void WaitFor(Predicate ready)
{
    const auto deadline=std::chrono::steady_clock::now()+2s;
    while(!ready() && std::chrono::steady_clock::now()<deadline) { std::this_thread::sleep_for(1ms); }
    assert(ready());
}
int main()
{
    BatteryStatusManager cache;
    std::atomic<int> reads{0};
    AsusBatteryMonitor monitor(cache,[&] {
        ++reads;
        return nlohmann::json{{"percentage",89},{"charging",nullptr},{"availability","receiver"}};
    },1h);
    monitor.SetEnabled(true,true); // A stale caller must not open without a live subscription.
    assert(reads==0);
    const auto token=cache.Acquire();
    monitor.SetEnabled(true,false); // No-wake validation is a separate hard gate.
    assert(reads==0 && cache.Snapshots()[1]["percentage"].is_null());
    monitor.SetEnabled(true,true);
    WaitFor([&]{return cache.Snapshots()[1]["percentage"]==89;});
    assert(reads==1);
    auto row=cache.Snapshots()[1];
    assert(row["availability"]=="receiver" && row["quality"]=="current" && row["charging"].is_null());
    monitor.SetEnabled(true,true);
    assert(reads==1); // Idempotent start does not create a second provider.
    const auto before=std::chrono::steady_clock::now();
    monitor.SetEnabled(false,true);
    assert(std::chrono::steady_clock::now()-before<500ms); // Interrupt the long backoff on unload.
    assert(cache.Snapshots()[1]["percentage"]==89 && cache.Snapshots()[1]["quality"]=="last_known");
    monitor.SetEnabled(true,true);
    WaitFor([&]{return reads==2;});
    monitor.SetEnabled(false,true);
    {
        AsusBatteryMonitor invalid(cache,[]{return nlohmann::json{{"percentage",101}};},1h);
        invalid.SetEnabled(true,true);
        WaitFor([&]{return cache.Snapshots()[1]["detail"]=="Invalid battery percentage reply";});
        assert(cache.Snapshots()[1]["percentage"]==89);
    }
    {
        AsusBatteryMonitor failing(cache,[]{throw std::runtime_error("transport unavailable");return nlohmann::json();},1h);
        failing.SetEnabled(true,true);
        WaitFor([&]{return cache.Snapshots()[1]["detail"]=="transport unavailable";});
        assert(cache.Snapshots()[1]["percentage"]==89);
    }
    cache.Release(token);
    std::cout<<"ASUS monitor: subscription/validation gates, one owner, receiver freshness, backoff cancellation and failure retention passed\n";
}
