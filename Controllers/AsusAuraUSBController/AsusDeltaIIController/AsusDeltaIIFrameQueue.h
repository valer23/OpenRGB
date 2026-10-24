/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "AsusDeltaIIProtocol.h"
#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>

// Pure frame policy: no USB/API access, timer thread or inferred awake state.
// The controller must supply independently validated, expiring awake evidence.
// Access is serialized by the controller; this queue is not a transport lock.
class AsusDeltaIIFrameQueue
{
public:
    using Time = std::chrono::steady_clock::time_point;
    using RGB = AsusDeltaIIProtocol::RGB;
    explicit AsusDeltaIIFrameQueue(std::chrono::milliseconds minimum_interval) : interval(minimum_interval)
    {
        if(interval.count() <= 0) { throw std::invalid_argument("Positive Delta II frame interval required"); }
    }
    uint64_t Generation() const { return generation; }
    void Suspend()
    {
        ++generation;
        permitted = false;
        pending.reset();
        emitted.reset();
        next = Time{};
    }
    // Retain the generation with the event source. A late callback from a
    // previous backend/session cannot authorize output after Suspend().
    void PermitUntil(uint64_t source_generation, Time deadline, Time now)
    {
        if(source_generation != generation) { return; }
        if(deadline <= now) { Suspend(); return; }
        if(permitted && now >= expires)
        {
            pending.reset();
            emitted.reset();
            next = Time{};
        }
        expires = deadline;
        permitted = true;
    }
    void Offer(const RGB& colour, Time now)
    {
        if(!Allowed(now)) { return; }
        if(emitted && *emitted == colour) { pending.reset(); }
        else { pending = colour; }
    }
    std::optional<AsusDeltaIIProtocol::Packet> Take(Time now)
    {
        if(!Allowed(now) || !pending || now < next) { return {}; }
        std::array<RGB, 14> colours;
        colours.fill(*pending); // One shared zone; no unvalidated LED mapping.
        auto frame = AsusDeltaIIProtocol::SoftwareFrame(colours);
        emitted = pending;
        pending.reset();
        next = now + interval;
        return frame;
    }
private:
    bool Allowed(Time now)
    {
        if(!permitted) { return false; }
        if(now >= expires) { Suspend(); return false; }
        return true;
    }
    std::chrono::milliseconds interval;
    Time expires{};
    Time next{};
    uint64_t generation = 0;
    bool permitted = false;
    std::optional<RGB> pending;
    std::optional<RGB> emitted;
};
