/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <future>
#include <map>
#include <string>
#include <vector>
#include "RGBControllerInterface.h"

using EffectDeviceFrame = std::map<unsigned int, RGBColor>;

// Stateful stock effects index cached data by assignment order. Only these
// audited effects can safely render a subset while slow devices are busy.
inline bool EffectSupportsPartialZones(const std::string& name)
{
    return name == "RainbowWave" || name == "Brimstone" || name == "IcueLayers" || name == "KeyRipple";
}

class EffectFrameQueue
{
public:
    bool Busy(RGBControllerInterface* controller) const
    {
        auto it = jobs.find(controller);
        return it != jobs.end() && it->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
    }
    template<class SubmitFunction>
    void Submit(const std::map<RGBControllerInterface*, EffectDeviceFrame>& frames, SubmitFunction submit)
    {
        for(auto it = jobs.begin(); it != jobs.end();)
        {
            if(it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                it->second.get();
                it = jobs.erase(it);
            }
            else ++it;
        }
        for(const auto& entry : frames)
        {
            if(jobs.find(entry.first) != jobs.end()) continue;
            // At most one outstanding job per device. A busy device skips
            // obsolete frames rather than building an animation backlog.
            jobs.emplace(entry.first, std::async(std::launch::async,
                [controller = entry.first, colors = entry.second, submit]() { submit(controller, colors); }));
        }
    }
private:
    // async futures join on destruction before controller assignments are released.
    std::map<RGBControllerInterface*, std::future<void>> jobs;
};

class EffectFrameBatch
{
public:
    EffectFrameBatch() { previous = current; current = this; }
    ~EffectFrameBatch() { current = previous; }
    EffectFrameBatch(const EffectFrameBatch&) = delete;
    EffectFrameBatch& operator=(const EffectFrameBatch&) = delete;
    static bool Active() { return current != nullptr; }
    static bool Stage(RGBControllerInterface* controller, unsigned int led, RGBColor color)
    {
        if(!current) return false;
        current->colors[controller][led] = color;
        return true;
    }
    template<class SubmitFunction>
    void Submit(EffectFrameQueue& queue, SubmitFunction submit)
    {
        current = previous;
        queue.Submit(colors, submit);
    }
private:
    inline static thread_local EffectFrameBatch* current = nullptr;
    EffectFrameBatch* previous = nullptr;
    std::map<RGBControllerInterface*, EffectDeviceFrame> colors;
};
