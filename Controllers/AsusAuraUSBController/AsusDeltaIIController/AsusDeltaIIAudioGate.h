/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "AsusDeltaIIFrameQueue.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

// Platform-neutral permission policy; no audio, USB or timer operations.
// A source must be qualified on the target hardware before enabling it.
class AsusDeltaIIAudioGate
{
public:
    using Time = std::chrono::steady_clock::time_point;
    struct Sample
    {
        uint64_t generation;
        Time observed;
        bool available;
        bool render_matches_receiver;
        unsigned int active_sessions;
        bool muted;
        float peak;
        float volume;
    };
    AsusDeltaIIAudioGate(std::chrono::milliseconds resume_delay,
                        std::chrono::milliseconds maximum_age) :
        delay(resume_delay), age(maximum_age)
    {
        if(delay.count() < 0 || age.count() <= 0)
            throw std::invalid_argument("Valid Delta II audio timing required");
    }
    void SetSource(uint64_t source_generation, bool normal_audio_wake_qualified)
    {
        generation = source_generation;
        qualified = normal_audio_wake_qualified;
        DisarmSession();
    }
    // Only an explicit awake confirmation or validated headset-on event arms
    // a session. Endpoint presence, battery data and audio alone never arm it.
    void ArmSession() { armed = true; ClearTiming(); }
    void DisarmSession() { armed = false; ClearTiming(); }
    std::optional<Time> Observe(const Sample& sample, Time now)
    {
        if(!armed || !qualified || sample.generation != generation ||
           !sample.available || !sample.render_matches_receiver ||
           !sample.active_sessions || sample.muted ||
           !std::isfinite(sample.peak) || sample.peak <= 0.0001f || sample.peak > 1 ||
           !std::isfinite(sample.volume) || sample.volume <= 0 || sample.volume > 1 ||
           sample.observed > now || now - sample.observed >= age)
        {
            ClearTiming();
            return {};
        }
        if(last && (sample.observed < *last || sample.observed - *last >= age))
            ClearTiming();
        if(!first) first = sample.observed;
        last = sample.observed;
        if(sample.observed - *first < delay) return {};
        // Re-reading a sample cannot extend its lease or settling duration.
        return sample.observed + age;
    }
    // Called on the same serialized controller path as Offer/Take, including
    // every invalid sample. Losing evidence revokes even an unexpired lease.
    void UpdateFramePermission(AsusDeltaIIFrameQueue& frames,
                               const Sample& sample, Time now)
    {
        const auto lease = Observe(sample, now);
        if(lease) frames.PermitUntil(frames.Generation(), *lease, now);
        else frames.Suspend();
    }
private:
    void ClearTiming() { first.reset(); last.reset(); }
    std::chrono::milliseconds delay;
    std::chrono::milliseconds age;
    uint64_t generation = 0;
    bool qualified = false;
    bool armed = false;
    std::optional<Time> first;
    std::optional<Time> last;
};
