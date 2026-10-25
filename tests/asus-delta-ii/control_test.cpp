/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIControl.h"
#include <cassert>
#include <deque>
#include <iostream>
using namespace std::chrono_literals;
using Control=AsusDeltaIIControl;
using Packet=AsusDeltaIIProtocol::Packet;
struct Fake : AsusDeltaIIReceiver::Io
{
    Control::Time time{10s};
    int opens=0,closes=0;
    bool busy=false,failed=false;
    std::deque<Packet> replies;
    std::vector<Packet> writes;
    bool Ready() override {return true;}
    bool Open(std::string& error) override {++opens;if(busy)error="vendor busy";return !busy;}
    void Close() override {++closes;}
    int Transfer(bool write,unsigned char* data,unsigned int size,unsigned int timeout) override
    {
        assert(size==64);
        if(write){Packet packet{};std::copy(data,data+64,packet.begin());writes.push_back(packet);if(failed)return -1;
            if(packet[2]!=0x30){Packet reply{};reply[0]=0xCC;reply[1]=0x51;reply[2]=packet[2];replies.push_back(reply);}return 64;}
        if(replies.empty()){time+=std::chrono::milliseconds(timeout);return 0;}
        std::copy(replies.front().begin(),replies.front().end(),data);replies.pop_front();return 64;
    }
};
int main()
{
    Fake io;AsusDeltaIIReceiver receiver(io);
    AsusDeltaIIDirectCommands direct(receiver,[&](){return io.time;});
    Control control(direct);
    std::string error;
    auto sample=[&](bool playing=true){return AsusDeltaIIAudioGate::Sample{7,io.time,true,true,playing?1U:0U,false,playing?0.5f:0.0f,1.0f};};
    auto tick=[&](bool playing=true){control.ObserveAudio(sample(playing),io.time);};
    assert(io.opens==0 && io.writes.empty());
    // ASUS is explicit and unavailable: it must never silently try the receiver.
    assert(!control.ApplyBuiltin(1,50,{89,0,164},io.time,error));
    assert(!control.BeginSoftware(50,{89,0,164},io.time,error));
    assert(io.opens==0 && !error.empty());
    control.SelectBackend(Control::Backend::DirectReceiver);
    control.Offer({1,2,3},io.time);assert(!control.Pump(io.time,error));
    assert(!control.BeginSoftware(50,{89,0,164},io.time,error)); // Unqualified audio source.
    assert(io.opens==0);
    control.SetAudioSource(7,true);
    assert(control.BeginSoftware(50,{89,0,164},io.time,error));
    const auto setup=io.writes.size();assert(setup==4);
    tick();control.Offer({255,0,0},io.time);assert(!control.Pump(io.time,error));
    // Continuous fresh audio settles for 3 seconds; no USB polling in this loop.
    for(int i=0;i<30;++i){io.time+=100ms;tick();}
    control.Offer({255,0,0},io.time);assert(control.Pump(io.time,error));
    assert(io.writes.size()==setup+1);
    io.time+=100ms;tick();control.Offer({0,255,0},io.time);
    control.Offer({0,0,255},io.time);assert(!control.Pump(io.time,error)); // coalesced and rate limited.
    for(int i=0;i<9;++i){io.time+=100ms;tick();}
    assert(control.Pump(io.time,error));assert(io.writes.back()[5]==0 && io.writes.back()[7]==255);
    const auto streamed=io.writes.size();
    io.time+=100ms;tick(false);control.Offer({99,88,77},io.time);
    assert(!control.Pump(io.time,error));assert(io.writes.size()==streamed);
    // Permission expiry without an intervening silence callback also drops queued frames.
    for(int i=0;i<31;++i){io.time+=100ms;tick();}
    control.Offer({99,88,77},io.time);io.time+=250ms;
    assert(!control.Pump(io.time,error));assert(io.writes.size()==streamed);
    for(int i=0;i<31;++i){io.time+=100ms;tick();}
    assert(!control.Pump(io.time,error)); // No stale colour replay when permission returns.
    control.Offer({3,4,5},io.time);assert(control.Pump(io.time,error));
    // Changing sources requires new explicit awake setup, never automatic reacquisition.
    control.SetAudioSource(8,true);const auto stopped=io.writes.size();const auto opened=io.opens;
    io.time+=4s;control.ObserveAudio({8,io.time,true,true,1,false,0.5f,1.0f},io.time);
    control.Offer({4,5,6},io.time);assert(!control.Pump(io.time,error));assert(io.opens==opened);
    assert(io.writes.size()==stopped);
    control.SelectBackend(Control::Backend::AsusService);
    assert(!control.ApplyOff(io.time,error));assert(io.opens==opened);
    control.SelectBackend(Control::Backend::DirectReceiver);
    assert(!control.ApplyOff(io.time-5s,error));assert(io.opens==opened);
    io.busy=true;assert(!control.ApplyOff(io.time,error));assert(io.writes.size()==stopped);io.busy=false;
    assert(control.ApplyBuiltin(2,50,{89,0,164},io.time,error));
    const auto builtin=io.writes.size();tick();control.Offer({7,8,9},io.time);
    assert(!control.Pump(io.time,error));assert(io.writes.size()==builtin);
    // Endpoint loss, receiver mismatch and late source generations cannot keep
    // a retained RGB session alive or trigger automatic reacquisition.
    for(int reason=0;reason<3;++reason)
    {
        control.SetAudioSource(7,true);assert(control.BeginSoftware(50,{89,0,164},io.time,error));
        for(int i=0;i<31;++i){io.time+=100ms;tick();}
        control.Offer({10,20,30},io.time);
        auto bad=sample();
        if(reason==0)bad.available=false;
        if(reason==1)bad.render_matches_receiver=false;
        if(reason==2)bad.generation=6;
        const auto beforeWrites=io.writes.size();const auto beforeOpens=io.opens;
        control.ObserveAudio(bad,io.time);
        for(int i=0;i<40;++i){io.time+=100ms;tick();control.Offer({30,20,10},io.time);assert(!control.Pump(io.time,error));}
        assert(io.writes.size()==beforeWrites && io.opens==beforeOpens);
    }
    control.SetAudioSource(7,true);assert(control.BeginSoftware(50,{89,0,164},io.time,error));
    for(int i=0;i<31;++i){io.time+=100ms;tick();}
    io.failed=true;control.Offer({9,8,7},io.time);assert(!control.Pump(io.time,error));io.failed=false;
    const auto failure=io.writes.size();const auto failureOpens=io.opens;
    for(int i=0;i<40;++i){io.time+=100ms;tick();control.Offer({1,2,3},io.time);assert(!control.Pump(io.time,error));}
    assert(io.writes.size()==failure && io.opens==failureOpens); // no retry or fallback.
    control.Stop();assert(io.writes.size()==failure); // no cleanup writes.
    std::cout<<"Delta II control: explicit backend, awake setup, audio settling/revocation, coalescing, 1Hz cadence, generation and failure guards passed\n";
}