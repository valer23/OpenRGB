/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIFrameQueue.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace std::chrono_literals;
using Queue = AsusDeltaIIFrameQueue;
int main()
{
    const Queue::Time start{};
    Queue frames(100ms); // Test interval only, not a validated hardware rate.
    const AsusDeltaIIProtocol::RGB red{255,0,0}, blue{0,0,255}, green{0,255,0};
    frames.Offer(red, start);
    assert(!frames.Take(start)); // No permission at construction.
    frames.PermitUntil(frames.Generation(), start + 1s, start);
    frames.Offer(red, start);
    auto first = frames.Take(start);
    assert(first && (*first)[5] == 255 && (*first)[7] == 0);
    frames.Offer(red, start + 20ms);
    assert(!frames.Take(start + 100ms)); // Identical colours are suppressed.
    frames.Offer(blue, start + 110ms);
    frames.Offer(green, start + 120ms);
    auto latest = frames.Take(start + 120ms);
    assert(latest && (*latest)[5] == 0 && (*latest)[6] == 255 && (*latest)[7] == 0);
    frames.Offer(blue, start + 130ms);
    assert(!frames.Take(start + 150ms)); // Bounded cadence.
    frames.Offer(green, start + 160ms); // Return to last sent colour drops blue.
    assert(!frames.Take(start + 230ms));
    frames.Offer(blue, start + 300ms);
    const auto old_source = frames.Generation();
    frames.Suspend(); // Disconnect, sleep, timeout or backend switch.
    frames.PermitUntil(old_source, start + 1s, start + 400ms);
    frames.Offer(red, start + 400ms);
    assert(!frames.Take(start + 400ms));
    frames.PermitUntil(frames.Generation(), start + 1s, start + 400ms);
    assert(!frames.Take(start + 500ms)); // No replay of dropped colours.
    frames.Offer(red, start + 900ms);
    assert(!frames.Take(start + 1s)); // Permission expires before send.
    frames.PermitUntil(frames.Generation(), start + 2s, start + 1100ms);
    assert(!frames.Take(start + 1200ms)); // Expiry discarded the pending frame.
    frames.Offer(blue, start + 1300ms);
    frames.PermitUntil(frames.Generation(), start, start + 1400ms); // Stale awake evidence revokes.
    assert(!frames.Take(start + 1500ms));
    frames.PermitUntil(frames.Generation(), start + 2s, start + 1600ms);
    frames.Offer(red, start + 1700ms);
    // A fresh permission arriving after expiry must not revive an old frame,
    // even if no timer called Take() during the expired interval.
    frames.PermitUntil(frames.Generation(), start + 3s, start + 2100ms);
    assert(!frames.Take(start + 2200ms));
    frames.Offer(blue, start + 2300ms);
    assert(frames.Take(start + 2300ms));
    std::cout << "Delta II frame queue: closed startup gate, deduplication, coalescing, expiry and sleep/drop semantics passed\n";
}
