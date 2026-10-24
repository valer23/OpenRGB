/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIAudioGate.h"
#include <cassert>
#include <limits>
#include <iostream>
using namespace std::chrono_literals;
int main()
{
    using Gate = AsusDeltaIIAudioGate;
    const Gate::Time zero{};
    Gate gate(3000ms, 250ms);
    Gate::Sample sample = {42, zero, true, true, 1, false, 0.5f, 0.5f};
    gate.SetSource(42, true);
    assert(!gate.Observe(sample, zero)); // Detection/source alone never arms RGB.
    gate.ArmSession();
    for(int i = 0; i < 30; ++i)
    {
        sample.observed = zero + i * 100ms;
        assert(!gate.Observe(sample, sample.observed));
    }
    sample.observed = zero + 3000ms;
    assert(gate.Observe(sample, sample.observed) == zero + 3250ms);
    assert(gate.Observe(sample, zero + 3100ms) == zero + 3250ms); // Re-read does not renew.
    assert(!gate.Observe(sample, zero + 3250ms));
    // A sample gap loses the settled state instead of reviving an old lease.
    sample.observed = zero + 4000ms;
    assert(!gate.Observe(sample, sample.observed));
    gate.SetSource(43, true);
    gate.ArmSession();
    assert(!gate.Observe(sample, sample.observed)); // Late old source.
    sample.generation = 43;
    for(int i = 0; i <= 30; ++i)
    {
        sample.observed = zero + 5000ms + i * 100ms;
        const auto lease = gate.Observe(sample, sample.observed);
        assert(static_cast<bool>(lease) == (i == 30));
    }
    sample.muted = true;
    assert(!gate.Observe(sample, sample.observed));
    sample.muted = false;
    assert(!gate.Observe(sample, sample.observed)); // Must settle again after mute.
    for(int scenario = 0; scenario < 8; ++scenario)
    {
        Gate invalid(3000ms, 250ms);
        invalid.SetSource(43, true); invalid.ArmSession();
        auto bad = sample;
        if(scenario == 0) bad.available = false;
        if(scenario == 1) bad.render_matches_receiver = false;
        if(scenario == 2) bad.active_sessions = 0;
        if(scenario == 3) bad.peak = 0;
        if(scenario == 4) bad.volume = 0;
        if(scenario == 5) bad.peak = std::numeric_limits<float>::quiet_NaN();
        if(scenario == 6) bad.volume = 1.1f;
        if(scenario == 7) bad.observed += 1ms;
        assert(!invalid.Observe(bad, sample.observed));
    }
    gate.SetSource(43, false); gate.ArmSession();
    assert(!gate.Observe(sample, sample.observed)); // Unqualified/unsupported platform.
    gate.SetSource(43, true); gate.ArmSession(); gate.DisarmSession();
    assert(!gate.Observe(sample, sample.observed));
    // Exercise the real gate-to-queue boundary after permission was granted.
    Gate coupled(0ms, 250ms);
    AsusDeltaIIFrameQueue frames(200ms);
    coupled.SetSource(43, true); coupled.ArmSession();
    sample.observed = zero + 9000ms;
    sample.peak = 0.5f;
    coupled.UpdateFramePermission(frames, sample, sample.observed);
    frames.Offer({1,2,3}, sample.observed);
    assert(frames.Take(sample.observed));
    frames.Offer({4,5,6}, sample.observed + 10ms);
    sample.muted = true;
    coupled.UpdateFramePermission(frames, sample, sample.observed + 20ms);
    assert(!frames.Take(sample.observed + 200ms)); // Revoke immediately, before lease expiry.
    sample.muted = false;
    sample.observed += 100ms;
    coupled.UpdateFramePermission(frames, sample, sample.observed);
    assert(!frames.Take(sample.observed)); // Revocation discarded the pending colour.
    frames.Offer({7,8,9}, sample.observed);
    coupled.DisarmSession();
    coupled.UpdateFramePermission(frames, sample, sample.observed);
    assert(!frames.Take(sample.observed));
    bool rejected = false;
    try { Gate invalid(-1ms, 250ms); } catch(const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    std::cout << "Delta II audio gate: closed startup, qualification, settling, freshness, source changes, mute and invalid data passed\n";
}
