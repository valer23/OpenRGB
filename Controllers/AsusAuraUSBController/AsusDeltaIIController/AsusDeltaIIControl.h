/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "AsusDeltaIIDirectCommands.h"
#include "AsusDeltaIIAudioGate.h"
#include <optional>

// Serialized control coordinator. Discovery, settings and Offer do no device I/O.
// The platform worker must use one owner thread for this object and direct I/O.
class AsusDeltaIIControl
{
public:
    using Time=std::chrono::steady_clock::time_point;
    using RGB=AsusDeltaIIProtocol::RGB;
    enum class Backend { AsusService, DirectReceiver };
    explicit AsusDeltaIIControl(AsusDeltaIIDirectCommands& direct) : direct(direct),
        frames(std::chrono::seconds(1)),
        audio(std::chrono::seconds(3),std::chrono::milliseconds(250)) {}
    ~AsusDeltaIIControl(){Stop();}
    AsusDeltaIIControl(const AsusDeltaIIControl&)=delete;
    AsusDeltaIIControl& operator=(const AsusDeltaIIControl&)=delete;

    void SelectBackend(Backend selected)
    {
        if(selected==backend)return;
        Stop();backend=selected;
    }
    void SetAudioSource(uint64_t generation,bool qualified)
    {
        if(generation==source && qualified==source_qualified)return;
        Stop();source=generation;source_qualified=qualified && generation!=0;
    }
    bool ApplyBuiltin(unsigned mode,unsigned brightness,RGB colour,Time confirmed_awake,std::string& error)
    {
        Stop();
        return DirectAvailable(error) && direct.ApplyBuiltin(mode,brightness,colour,confirmed_awake,error);
    }
    bool ApplyOff(Time confirmed_awake,std::string& error)
    {
        Stop();
        return DirectAvailable(error) && direct.ApplyOff(confirmed_awake,error);
    }
    bool BeginSoftware(unsigned brightness,RGB colour,Time confirmed_awake,std::string& error)
    {
        Stop();
        if(!DirectAvailable(error))return false;
        if(!source_qualified || source==0)
        {
            error="Qualified Delta II audio source required before software RGB";
            return false;
        }
        if(!direct.BeginSoftware(brightness,colour,confirmed_awake,error))return false;
        audio.SetSource(source,true);audio.ArmSession();streaming=true;
        return true;
    }
    void ObserveAudio(const AsusDeltaIIAudioGate::Sample& sample,Time now)
    {
        if(!streaming)return;
        if(sample.generation!=source || !sample.available || !sample.render_matches_receiver)
        {
            Stop(); // Endpoint loss/replacement needs fresh explicit awake setup.
            return;
        }
        permission=audio.Observe(sample,now);
        if(permission)frames.PermitUntil(frames.Generation(),*permission,now);
        else frames.Suspend(); // Silence/mute immediately discards pending colours.
    }
    void Offer(RGB colour,Time now)
    {
        if(streaming)frames.Offer(colour,now);
    }
    bool Pump(Time now,std::string& error)
    {
        error.clear();
        if(!streaming || !permission || now>=*permission)
        {
            frames.Suspend();permission.reset();return false;
        }
        const auto frame=frames.Take(now);
        if(!frame)return false;
        const RGB colour{(*frame)[5],(*frame)[6],(*frame)[7]};
        if(direct.WriteFrame(colour,*permission,error))return true;
        Stop(); // No retries, automatic setup, fallback or cleanup writes.
        return false;
    }
    void Stop()
    {
        streaming=false;permission.reset();frames.Suspend();audio.DisarmSession();direct.Stop();
    }
private:
    bool DirectAvailable(std::string& error) const
    {
        error.clear();
        if(backend==Backend::DirectReceiver)return true;
        error="ASUS existing-owner RGB integration is unavailable; no direct fallback";
        return false;
    }
    AsusDeltaIIDirectCommands& direct;
    AsusDeltaIIFrameQueue frames;
    AsusDeltaIIAudioGate audio;
    Backend backend=Backend::AsusService;
    uint64_t source=0;
    bool source_qualified=false,streaming=false;
    std::optional<Time> permission;
};