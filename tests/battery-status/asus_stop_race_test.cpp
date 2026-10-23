#include "BatteryStatus/AsusBatteryMonitor.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
namespace {
std::atomic<bool> paused{false}, release_predicate{false}, notified{false}, armed{true};
template<class Predicate> void WaitFor(Predicate ready) {
    const auto deadline=std::chrono::steady_clock::now()+2s;
    while(!ready() && std::chrono::steady_clock::now()<deadline) { std::this_thread::sleep_for(1ms); }
    assert(ready());
}
}
void AsusPredicateTestHook(bool stopping) {
    if(stopping || !armed.exchange(false)) { return; }
    paused.store(true);
    while(!release_predicate.load()) { std::this_thread::yield(); }
}
void AsusNotifyTestHook() { notified.store(true); }
int main() {
    BatteryStatusManager cache;
    const auto token=cache.Acquire();
    AsusBatteryMonitor monitor(cache,[]{return nlohmann::json{{"percentage",89}};},1s);
    monitor.SetEnabled(true,true);
    WaitFor([]{return paused.load();});
    // Hold the worker between predicate=false and wait while disable executes.
    std::thread disabling([&]{monitor.SetEnabled(false,true);});
    const auto deadline=std::chrono::steady_clock::now()+100ms;
    while(!notified.load() && std::chrono::steady_clock::now()<deadline) { std::this_thread::yield(); }
    const auto released_at=std::chrono::steady_clock::now();
    release_predicate.store(true);
    disabling.join();
    const auto elapsed=std::chrono::steady_clock::now()-released_at;
    cache.Release(token);
    assert(elapsed<300ms); // A lost notification waits the full1s interval.
    std::cout<<"ASUS disable interleaving: predicate/wait race cannot lose shutdown notification\n";
}
