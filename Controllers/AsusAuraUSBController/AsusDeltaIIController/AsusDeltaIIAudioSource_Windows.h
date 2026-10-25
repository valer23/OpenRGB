/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "AsusDeltaIIAudioGate.h"
#include <memory>
#include <string>
#include <vector>

// Windows-only read-only source. Use Open/Read/Stop/destruction on the same
// worker thread. Construction creates no COM object, audio stream or USB handle.
class AsusDeltaIIAudioSource
{
public:
    using Clock=std::chrono::steady_clock;
    using Time=Clock::time_point;
    struct Endpoint {std::string id,container;bool render,active;};
    static std::string SelectEndpoint(const std::string& receiver,const std::vector<Endpoint>& candidates)
    {
        if(receiver.empty())return {};
        std::string selected;unsigned matches=0;
        for(const auto& endpoint:candidates)
            if(endpoint.container==receiver && endpoint.render && endpoint.active)
            {if(endpoint.id.empty())return {};++matches;selected=endpoint.id;}
        return matches==1 ? selected : std::string{};
    }
    AsusDeltaIIAudioSource();
    ~AsusDeltaIIAudioSource();
    AsusDeltaIIAudioSource(const AsusDeltaIIAudioSource&)=delete;
    AsusDeltaIIAudioSource& operator=(const AsusDeltaIIAudioSource&)=delete;
    bool Open(Time confirmed_awake,std::string& error);
    AsusDeltaIIAudioGate::Sample Read();
    uint64_t Generation() const;
    void Stop();
private:
    struct State;
    std::unique_ptr<State> state;
};