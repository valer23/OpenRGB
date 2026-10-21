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
    {
        BatteryStatusManager cached;
        std::atomic<int> files{0},usb{0};
        BatteryMonitoringService passive(cached,[&] { ++files; return nlohmann::json{
            {"source","asus_log"},{"source_age_ms",8000},{"availability","asus_cache_offline"},
            {"percentage",83},{"charging",false},{"detail","Existing ASUS report"}}; },false,1h,[&] {
                ++usb; return nlohmann::json();
            },true);
        const auto token=passive.Acquire();
        auto deadline=std::chrono::steady_clock::now()+2s;
        while(cached.Snapshots()[1]["percentage"].is_null() && std::chrono::steady_clock::now()<deadline) { std::this_thread::sleep_for(1ms); }
        auto row=cached.Snapshots()[1];
        assert(files==1 && usb==0 && row["percentage"]==83);
        assert(row["acquisition"]=="asus_log" && row["quality"]=="last_known" && row["age_ms"]>=8000);
        assert(row["availability"]=="asus_cache_offline");
        assert(!passive.RequestAsusRefresh(token,true) && !passive.RequestAsusRefresh(token,false));
        passive.SetSuspended(true);passive.SetSuspended(false);
        deadline=std::chrono::steady_clock::now()+2s;
        while(files<2 && std::chrono::steady_clock::now()<deadline) { std::this_thread::sleep_for(1ms); }
        passive.Release(token);
        assert(files==2 && usb==0);
    }
    // Explicit cache/direct switching never grants permission for a USB read.
    {
        BatteryStatusManager modes;
        std::atomic<int> files{0},usb{0};
        BatteryMonitoringService service(modes,[&] {
            ++files;return nlohmann::json{{"error","Existing ASUS log unavailable"}};
        },false,1ms,[&] {
            ++usb;return nlohmann::json{{"percentage",70},{"charging",false}};
        },true);
        auto token=service.Acquire();
        auto deadline=std::chrono::steady_clock::now()+2s;
        while(files<2 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
        assert(files>=2 && usb==0);
        assert(!service.RequestAsusRefresh(token,true));
        assert(!service.SetAsusMode("auto"));
        assert(service.SetAsusMode("direct"));
        const auto stopped_files=files.load();
        assert(modes.Snapshots()[1]["acquisition"]=="manual");
        service.SetSuspended(true);service.SetSuspended(false);
        auto second=service.Acquire();service.Release(second);
        std::this_thread::sleep_for(10ms);
        assert(usb==0 && files==stopped_files);
        assert(!service.RequestAsusRefresh(token,false));
        assert(service.RequestAsusRefresh(token,true));
        deadline=std::chrono::steady_clock::now()+2s;
        while(modes.Snapshots()[1]["percentage"].is_null() && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
        assert(usb==1 && modes.Snapshots()[1]["percentage"]==70);
        assert(service.SetAsusMode("cache"));
        assert(modes.Snapshots()[1]["percentage"].is_null()); // Do not relabel a direct sample as an ASUS report.
        assert(!service.RequestAsusRefresh(token,true));
        service.Release(token);
        assert(usb==1 && modes.Snapshots()[1]["acquisition"]=="asus_log");
    }
    // A mode change joins an in-flight manual request and discards its result.
    {
        BatteryStatusManager modes;
        std::promise<void> entered,finish;
        auto done=finish.get_future().share();
        BatteryMonitoringService service(modes,[] { return nlohmann::json{{"error","No ASUS cache"}}; },
            false,1h,[&] { entered.set_value();done.wait();return nlohmann::json{{"percentage",99}}; });
        const auto token=service.Acquire();
        assert(service.RequestAsusRefresh(token,true));
        assert(entered.get_future().wait_for(2s)==std::future_status::ready);
        auto changed=std::async(std::launch::async,[&] { return service.SetAsusMode("cache"); });
        assert(changed.wait_for(30ms)==std::future_status::timeout);
        finish.set_value();
        assert(changed.wait_for(2s)==std::future_status::ready && changed.get());
        assert(modes.Snapshots()[1]["percentage"].is_null());
        assert(!service.RequestAsusRefresh(token,true));
        service.Release(token);
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
