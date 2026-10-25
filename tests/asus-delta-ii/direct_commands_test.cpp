/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIDirectCommands.h"
#include <cassert>
#include <deque>
#include <iostream>
#include <future>
using namespace std::chrono_literals;
using Packet = AsusDeltaIIProtocol::Packet;
struct Fake : AsusDeltaIIReceiver::Io
{
    int opens=0, closes=0;
    bool busy=false, fail_write=false, off=false, missing_ack=false, stalled=false;
    std::deque<Packet> replies;
    std::vector<Packet> writes;
    std::function<void()> on_write;
    AsusDeltaIIDirectCommands::Time now{10s};
    bool Ready() override { return true; }
    bool Open(std::string& error) override { ++opens; if(busy) error="busy"; return !busy; }
    void Close() override { ++closes; }
    int Transfer(bool write, unsigned char* data, unsigned int size, unsigned int timeout) override
    {
        assert(size==64);
        if(stalled) now+=6s;
        if(write)
        {
            Packet packet{};std::copy(data,data+64,packet.begin());writes.push_back(packet);
            if(on_write)on_write();
            if(fail_write)return -1;
            if(!missing_ack && packet[2]!=0x30){Packet ack{};ack[0]=0xCC;ack[1]=0x51;ack[2]=packet[2];replies.push_back(ack);}
            return 64;
        }
        if(off){off=false;Packet state{};state[0]=0xCC;state[1]=0x12;state[4]=1;std::copy(state.begin(),state.end(),data);return 64;}
        if(!replies.empty()){std::copy(replies.front().begin(),replies.front().end(),data);replies.pop_front();return 64;}
        now+=std::chrono::milliseconds(timeout);return 0;
    }
};
int main()
{
    Fake io;AsusDeltaIIReceiver receiver(io);
    AsusDeltaIIDirectCommands commands(receiver,[&](){return io.now;});
    std::string error;
    assert(io.opens==0 && io.writes.empty());
    assert(!commands.ApplyBuiltin(1,50,{89,0,164},io.now-5s,error));
    assert(!commands.ApplyBuiltin(1,50,{89,0,164},io.now+1ms,error));
    assert(!commands.ApplyBuiltin(6,50,{89,0,164},io.now,error));
    assert(io.opens==0);
    assert(commands.ApplyBuiltin(2,50,{89,0,164},io.now,error));
    assert(io.writes.size()==3 && io.writes[0][2]==0x33 && io.writes[1][2]==0x10 && io.writes[2][2]==0x28);
    assert(io.closes==1);
    io.writes.clear();assert(commands.ApplyOff(io.now,error));
    assert(io.writes.size()==2 && io.writes[1][5]==0);
    io.writes.clear();assert(commands.BeginSoftware(50,{89,0,164},io.now,error));
    assert(io.writes.size()==4 && io.writes[0][2]==0x10 && io.writes[3][2]==0x30);
    // A manually requested battery transaction borrows the retained owner.
    const auto opened=io.opens;
    {auto battery=receiver.Acquire(error);assert(battery);}
    assert(io.opens==opened);
    assert(!commands.WriteFrame({1,2,3},io.now,error)); // Expired permission.
    assert(io.writes.size()==4);
    assert(commands.WriteFrame({1,2,3},io.now+250ms,error));
    assert(io.writes.size()==5);
    commands.Stop();const auto writes=io.writes.size();
    assert(!commands.WriteFrame({4,5,6},io.now+250ms,error));
    assert(io.writes.size()==writes); // Stop never sends cleanup or reacquires.
    // Failures inside the retained transaction must close it without recursively
    // invoking receiver.Release() while its nonrecursive mutex is held.
    for(int failure=0;failure<3;++failure)
    {
        io.writes.clear();io.replies.clear();
        assert(commands.BeginSoftware(50,{1,2,3},io.now,error));
        const auto beforeClose=io.closes;
        if(failure==0)io.off=true;
        if(failure==1)io.fail_write=true;
        if(failure==2)io.stalled=true;
        const auto lease=io.now+250ms;
        assert(!commands.WriteFrame({4,5,6},lease,error));
        assert(io.closes==beforeClose+1);
        io.off=false;io.fail_write=false;io.stalled=false;io.replies.clear();
        const auto after=io.writes.size();
        assert(!commands.WriteFrame({4,5,6},io.now+250ms,error));
        assert(io.writes.size()==after);
    }
    // Losing the retained session through battery failure or an external
    // release never permits a frame to reopen a receiver without awake setup.
    for(int loss=0;loss<2;++loss)
    {
        io.writes.clear();io.replies.clear();
        assert(commands.BeginSoftware(50,{1,2,3},io.now,error));
        if(loss==0)
        {
            auto battery=receiver.Acquire(error);assert(battery);
            io.fail_write=true;Packet packet{};
            assert(battery.Transfer(true,packet.data(),64,100)==-1);
            io.fail_write=false;
        }
        else receiver.Release();
        const auto opens=io.opens;const auto writes=io.writes.size();
        assert(!commands.WriteFrame({4,5,6},io.now+250ms,error));
        assert(io.opens==opens && io.writes.size()==writes);
    }
    io.replies.clear();assert(commands.BeginSoftware(50,{1,2,3},io.now,error));
    io.on_write=[&](){auto invalidator=std::async(std::launch::async,[&](){receiver.Release();});invalidator.get();};
    assert(!commands.WriteFrame({4,5,6},io.now+250ms,error));
    io.on_write={};
    io.replies.clear();assert(commands.BeginSoftware(50,{1,2,3},io.now,error));
    std::promise<void> borrowed,done;
    auto resume=done.get_future();
    auto battery=std::async(std::launch::async,[&](){auto tx=receiver.Acquire(error);assert(tx);borrowed.set_value();resume.wait();});
    borrowed.get_future().wait();
    assert(!commands.WriteFrame({4,5,6},io.now+250ms,error));
    done.set_value();battery.get();
    const auto closed=io.closes;commands.Stop();
    assert(io.closes==closed+1); // Busy refusal cannot orphan the retained owner.
    io.writes.clear();io.off=true;
    assert(!commands.BeginSoftware(50,{1,2,3},io.now,error));assert(io.writes.empty());
    io.writes.clear();io.missing_ack=true;
    assert(!commands.ApplyBuiltin(1,50,{1,2,3},io.now,error));assert(io.writes.size()==1);
    io.missing_ack=false;io.replies.clear();io.writes.clear();io.fail_write=true;
    assert(!commands.ApplyOff(io.now,error));assert(io.writes.size()==1);
    io.fail_write=false;io.writes.clear();io.stalled=true;
    assert(!commands.ApplyOff(io.now,error));assert(io.writes.empty());
    io.stalled=false;io.busy=true;const auto before=io.opens;
    assert(!commands.ApplyOff(io.now,error));assert(io.opens==before+1 && io.writes.empty());
    std::cout<<"Delta II direct commands: no startup output, fresh awake intent, exact packets/ACKs, shared owner, expiry, disconnect, no retry and no cleanup passed\n";
}
