#include "BatteryStatus/BatteryMonitoringService.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <future>
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
    // One awake-confirmed request runs off the caller thread, without polling.
    {
        BatteryStatusManager manual_cache;
        std::promise<void> entered, finish;
        auto finished = finish.get_future().share();
        std::atomic<int> manual_reads{0};
        auto once = [&] { ++manual_reads; entered.set_value(); finished.wait();
            return nlohmann::json{{"percentage",73},{"charging",false}}; };
        BatteryMonitoringService manual(manual_cache, reader, false, 1ms, once);
        assert(!manual.RequestAsusRefresh(0,true));
        const auto token = manual.Acquire();
        assert(!manual.RequestAsusRefresh(token,false));
        assert(!manual.RequestAsusRefresh(token+100,true));
        manual.SetSuspended(true);
        assert(!manual.RequestAsusRefresh(token,true));
        manual.SetSuspended(false);
        assert(manual_reads == 0);
        assert(manual.RequestAsusRefresh(token,true));
        assert(entered.get_future().wait_for(2s) == std::future_status::ready);
        assert(manual_cache.Snapshots()[1]["availability"] == "refreshing");
        assert(!manual.RequestAsusRefresh(token,true));
        auto other = manual.Acquire();
        manual.Release(other); // A second subscriber cannot cancel this request.
        finish.set_value();
        auto deadline = std::chrono::steady_clock::now()+2s;
        while(manual_cache.Snapshots()[1]["percentage"].is_null() && std::chrono::steady_clock::now()<deadline) { std::this_thread::sleep_for(1ms); }
        assert(manual_cache.Snapshots()[1]["percentage"] == 73);
        assert(manual_cache.Snapshots()[1]["quality"] == "last_known");
        std::this_thread::sleep_for(15ms);
        assert(manual_reads == 1);
        manual.Release(token);
        assert(!manual.RequestAsusRefresh(token,true));
    }
    {
        BatteryStatusManager manual_cache;
        int attempts=0;
        BatteryMonitoringService manual(manual_cache,reader,false,1ms,[&] {
            if(++attempts==1) { return nlohmann::json{{"percentage",65}}; }
            return nlohmann::json{{"error","Receiver busy; no competing reader opened"}};
        });
        const auto token=manual.Acquire();
        assert(manual.RequestAsusRefresh(token,true));
        auto deadline=std::chrono::steady_clock::now()+2s;
        while(manual_cache.Snapshots()[1]["availability"]=="refreshing" && std::chrono::steady_clock::now()<deadline) { std::this_thread::sleep_for(1ms); }
        assert(manual.RequestAsusRefresh(token,true));
        deadline=std::chrono::steady_clock::now()+2s;
        while(manual_cache.Snapshots()[1]["availability"]=="refreshing" && std::chrono::steady_clock::now()<deadline) { std::this_thread::sleep_for(1ms); }
        const auto row=manual_cache.Snapshots()[1];
        assert(row["percentage"]==65 && row["quality"]=="last_known");
        assert(row["detail"].get<std::string>().find("busy")!=std::string::npos);
        manual.Release(token);
    }
    {
        BatteryStatusManager manual_cache;
        BatteryMonitoringService manual(manual_cache,reader,false,1ms,[] {
            return nlohmann::json{{"percentage",71},{"charging",nullptr},{"charging_error","Charging query timed out"}};
        });
        auto token=manual.Acquire();
        assert(manual.RequestAsusRefresh(token,true));
        auto deadline=std::chrono::steady_clock::now()+2s;
        while(manual_cache.Snapshots()[1]["availability"]=="refreshing" && std::chrono::steady_clock::now()<deadline) { std::this_thread::sleep_for(1ms); }
        auto row=manual_cache.Snapshots()[1];
        assert(row["percentage"]==71 && row["charging"].is_null());
        assert(row["detail"].get<std::string>().find("Charging query timed out")!=std::string::npos);
        manual.Release(token);
    }
    // Final release waits for core-owned work and discards its late result.
    {
        BatteryStatusManager manual_cache;
        manual_cache.Publish(BatteryStatusManager::AsusId,40,-1,"manual");
        std::promise<void> entered, finish;
        auto finished=finish.get_future().share();
        BatteryMonitoringService manual(manual_cache,reader,false,1ms,[&] {
            entered.set_value(); finished.wait(); return nlohmann::json{{"percentage",90}};
        });
        const auto token=manual.Acquire();
        assert(manual.RequestAsusRefresh(token,true));
        assert(entered.get_future().wait_for(2s)==std::future_status::ready);
        auto released=std::async(std::launch::async,[&] { manual.Release(token); });
        assert(released.wait_for(30ms)==std::future_status::timeout);
        finish.set_value();
        assert(released.wait_for(2s)==std::future_status::ready);
        released.get();
        assert(!manual_cache.Enabled());
        assert(manual_cache.Snapshots()[1]["percentage"]==40);
    }
    std::cout<<"Core monitoring service: closed hardware gate, shared subscription ownership and final-release join passed\n";
}
