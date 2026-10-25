/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>

// Pure policy, serialized by the Windows observer. No HID access or input polling.
class CorsairStartupRecoveryPolicy
{
public:
    using Time = std::chrono::steady_clock::time_point;
    struct Request { std::string path, container; Time observed; uint64_t epoch, attempt; };
    void Watch(const std::string& path, const std::string& container)
    {
        if(path.empty() || container.empty()) return;
        for(auto it=targets.begin();it!=targets.end();)
        {
            if(it->first!=path && it->second.container==container) it=targets.erase(it);
            else ++it;
        }
        const auto found=targets.find(path);
        if(found!=targets.end() && found->second.container==container) return;
        targets[path]=Target{container};
    }
    std::optional<Request> Input(const std::string& container, Time observed, Time now)
    {
        if(!Fresh(observed,now)) return {};
        for(auto& entry:targets)
        {
            auto& target=entry.second;
            if(target.container!=container || target.pending ||
               (target.last && now-*target.last<std::chrono::seconds(2))) continue;
            target.last=now;target.pending=true;target.attempt=++attempt;
            return Request{entry.first,container,observed,epoch,target.attempt};
        }
        return {};
    }
    bool Current(const Request& request, Time now) const { return Matches(request) && Fresh(request.observed,now); }
    void Finished(const Request& request,bool recovered)
    {
        if(!Matches(request)) return;
        if(recovered) targets.erase(request.path);
        else targets.at(request.path).pending=false;
    }
    void Forget(const std::string& path) { targets.erase(path); }
    void ForgetReceiver(const std::string& container)
    {
        for(auto it=targets.begin();it!=targets.end();)
            if(it->second.container==container)it=targets.erase(it);else ++it;
    }
    void Clear() { ++epoch;targets.clear(); }
    bool Empty() const { return targets.empty(); }
private:
    struct Target { std::string container; std::optional<Time> last{}; bool pending=false; uint64_t attempt=0; };
    static bool Fresh(Time observed,Time now) { return observed<=now && now-observed<std::chrono::seconds(1); }
    bool Matches(const Request& request) const
    {
        const auto found=targets.find(request.path);
        return request.epoch==epoch && found!=targets.end() && found->second.pending &&
               found->second.container==request.container && found->second.attempt==request.attempt;
    }
    std::map<std::string,Target> targets;
    uint64_t epoch=0,attempt=0;
};

// DetectionManager's existing worker owns a single slot and holds its state
// mutex while executing work. Recovery must neither wait nor overwrite it.
inline bool TryScheduleCorsairRecovery(std::mutex& mutex,std::function<void()>& slot,
                                      bool running,std::function<void()> work)
{
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
    if(!lock.owns_lock() || !running || slot) return false;
    slot=std::move(work);
    return true;
}
inline uint32_t CorsairMouseInputAge(uint32_t now,uint32_t created) { return now-created; }
inline bool CorsairMouseInputMeaningful(long x,long y,unsigned short buttons) { return x || y || buttons; }

// Shared by the real detection worker and injected metadata/lifecycle tests.
inline bool RunCorsairRecoveryAttempt(const std::function<bool()>& current,
    const std::function<bool()>& registered,const std::function<bool()>& enabled,
    const std::function<bool()>& lookup,const std::function<void()>& detect)
{
    if(!current())return false;
    if(registered() || !enabled())return true;
    if(!lookup() || !current())return false;
    detect();
    return registered();
}

// Bridge the existing two-argument detector callback to its ordinary controller
// constructor. This scope carries input evidence; it opens no reader. Ownership
// remains with the controller's existing HID handle.
class CorsairRecoveryInputScope
{
public:
    explicit CorsairRecoveryInputScope(const std::function<bool()>& check) : check(check),previous(active)
    { active=this->check ? this : nullptr; }
    ~CorsairRecoveryInputScope(){active=previous;}
    CorsairRecoveryInputScope(const CorsairRecoveryInputScope&)=delete;
    CorsairRecoveryInputScope& operator=(const CorsairRecoveryInputScope&)=delete;
    static bool Requested(){return active!=nullptr;}
    static bool Current(){return active && active->check();}
    static void MarkAmbiguous(){if(active)active->ambiguous=true;}
    static bool Ambiguous(){return active && active->ambiguous;}
private:
    std::function<bool()> check;
    CorsairRecoveryInputScope* previous;
    bool ambiguous=false;
    inline static thread_local CorsairRecoveryInputScope* active=nullptr;
};
