/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BatteryStatus/AsusDeltaIIBattery.h"
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIDirectCommands.h"
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIReceiver.h"
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIProtocol.h"
#include <cassert>
#include <deque>
#include <future>
#include <functional>
#include <iostream>
#include <vector>
using Packet = AsusDeltaIIProtocol::Packet;
struct SimulatedReceiver final : AsusDeltaIIReceiver::Io
{
    bool ready = true, available = true, opened = false;
    int opens = 0, closes = 0;
    std::deque<Packet> replies;
    std::vector<Packet> writes;
    std::function<void()> on_write;
    bool Ready() override { return ready; }
    bool Open(std::string& error) override
    {
        ++opens;
        if(!available) { error = "Competing reader"; return false; }
        opened = true;
        return true;
    }
    void Close() override { if(opened) { ++closes; opened = false; } }
    int Transfer(bool writing, unsigned char* data, unsigned int size, unsigned int) override
    {
        assert(opened && size == 64);
        if(writing)
        {
            Packet packet = {}; std::copy(data, data + size, packet.begin());
            writes.push_back(packet);
            if(on_write) { on_write(); }
        }
        else
        {
            if(replies.empty()) { return 0; }
            std::copy(replies.front().begin(), replies.front().end(), data);
            replies.pop_front();
        }
        return 64;
    }
} io;
AsusDeltaIIReceiver& GetAsusDeltaIIReceiver()
{
    static AsusDeltaIIReceiver receiver(io);
    return receiver;
}
int main()
{
    auto denied = AsusDeltaIIBattery::ReadAwakeSnapshot(false);
    assert(denied["percentage"].is_null() && io.opens == 0 && io.writes.empty());
    std::string error;
    {
        auto rgb = GetAsusDeltaIIReceiver().Acquire(error);
        assert(rgb);
        rgb.KeepOpen(true);
    }
    Packet old_rgb_ack = {}; old_rgb_ack[0] = 0xCC; old_rgb_ack[1] = 0x51; old_rgb_ack[2] = 0x33;
    Packet level = {}; level[0] = 0xCC; level[1] = 0x12; level[2] = 7; level[6] = 84;
    Packet charging = {}; charging[0] = 0xCC; charging[1] = 0x12; charging[2] = 8; charging[5] = 1;
    io.replies = {old_rgb_ack, level, level, charging};
    auto snapshot = AsusDeltaIIBattery::ReadAwakeSnapshot(true);
    assert(snapshot["percentage"] == 84 && snapshot["charging"] == true);
    assert(io.opens == 1 && io.closes == 0 && io.writes.size() == 2);
    assert(io.writes[0][1] == 0x12 && io.writes[0][2] == 7 && io.writes[1][2] == 8);
    {
        auto rgb = GetAsusDeltaIIReceiver().Acquire(error);
        auto other = std::async(std::launch::async, [] { return AsusDeltaIIBattery::ReadAwakeSnapshot(true); });
        auto busy = other.get();
        assert(busy["percentage"].is_null() && busy.contains("error"));
        assert(io.opens == 1 && io.writes.size() == 2);
        rgb.KeepOpen(false);
    }
    assert(io.closes == 1);
    io.available = false;
    auto unavailable = AsusDeltaIIBattery::ReadAwakeSnapshot(true);
    assert(unavailable["error"] == "Competing reader" && io.writes.size() == 2);
    io.available = true; io.ready = false;
    int attempts = io.opens;
    auto pending = AsusDeltaIIBattery::ReadAwakeSnapshot(true);
    assert(pending["percentage"].is_null() && io.opens == attempts && io.writes.size() == 2);
    io.ready = true;
    std::promise<void> written, resume;
    auto continue_reading = resume.get_future();
    io.on_write = [&] { written.set_value(); continue_reading.wait(); };
    auto reading = std::async(std::launch::async, [] { return AsusDeltaIIBattery::ReadAwakeSnapshot(true); });
    written.get_future().wait();
    GetAsusDeltaIIReceiver().Release();
    resume.set_value();
    const auto interrupted = reading.get();
    assert(interrupted["percentage"].is_null() && interrupted.contains("error"));
    assert(io.writes.size() == 3 && io.writes.back()[2] == 7); // No charging query after release.
    io.on_write=[&]()
    {
        const auto& packet=io.writes.back();
        if(packet[1]==0x51 && packet[2]!=0x30)
        {Packet ack{};ack[0]=0xCC;ack[1]=0x51;ack[2]=packet[2];io.replies.push_back(ack);}
    };
    io.replies.clear();
    AsusDeltaIIDirectCommands commands(GetAsusDeltaIIReceiver());
    const auto now=std::chrono::steady_clock::now();
    assert(commands.BeginSoftware(50,{89,0,164},now,error));
    Packet connected{};connected[0]=0xCC;connected[1]=0x12;connected[4]=1;connected[5]=1;
    io.replies={connected,level,charging};
    const auto rgb_opens=io.opens;
    auto still_connected=AsusDeltaIIBattery::ReadAwakeSnapshot(true);
    assert(still_connected["percentage"]==84 && io.opens==rgb_opens);
    assert(commands.WriteFrame({1,2,3},std::chrono::steady_clock::now()+std::chrono::milliseconds(250),error));
    Packet disconnected=connected;disconnected[5]=0;
    io.replies={disconnected};
    auto off=AsusDeltaIIBattery::ReadAwakeSnapshot(true);
    assert(off["percentage"].is_null());
    const auto off_writes=io.writes.size();
    // The actual battery path consumed OFF. RGB still must lose its retained
    // session, send no leased frame and never open a replacement reader.
    assert(!commands.WriteFrame({4,5,6},std::chrono::steady_clock::now()+std::chrono::milliseconds(250),error));
    assert(io.writes.size()==off_writes && io.opens==rgb_opens);
    assert(commands.BeginSoftware(50,{89,0,164},std::chrono::steady_clock::now(),error));
    io.replies={disconnected};
    const auto observer_writes=io.writes.size();const auto observer_opens=io.opens;
    const auto observed=AsusDeltaIIBattery::Observe();
    assert(observed["commands_sent"]==0 && observed.contains("error"));
    assert(observed["reports"].size()==1 && observed["reports"][0][0]==0xCC);
    assert(!commands.WriteFrame({7,8,9},std::chrono::steady_clock::now()+std::chrono::milliseconds(250),error));
    assert(io.writes.size()==observer_writes && io.opens==observer_opens);
    std::cout << "Actual battery path: shares RGB session, routes queued replies, rejects contention and pending cancellation\n";
}
