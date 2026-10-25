/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/RGBController_AsusDeltaII_Windows.h"
#include <cassert>
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIDetection.h"
#include <atomic>
#include <deque>
#include <iostream>
using namespace std::chrono_literals;
using Packet=AsusDeltaIIProtocol::Packet;
std::atomic<int> audio_opens{0},audio_reads{0};
std::atomic<bool> playing{true};
struct FakeIo : AsusDeltaIIReceiver::Io
{
    std::atomic<int> opens{0},writes{0};std::deque<Packet> replies;
    bool Ready()override{return true;}
    bool Open(std::string&)override{++opens;return true;}
    void Close()override{}
    int Transfer(bool write,unsigned char* data,unsigned int size,unsigned int)override
    {
        assert(size==64);
        if(write){++writes;if(data[2]!=0x30){Packet ack{};ack[0]=0xCC;ack[1]=0x51;ack[2]=data[2];replies.push_back(ack);}return 64;}
        if(replies.empty())return 0;
        std::copy(replies.front().begin(),replies.front().end(),data);replies.pop_front();return 64;
    }
} io;
AsusDeltaIIReceiver& GetAsusDeltaIIReceiver(){static AsusDeltaIIReceiver receiver(io);return receiver;}
// Replace only the OS audio provider. The real controller, base device worker,
// RGB/settings interface, coordinator, receiver and command path are linked.
struct AsusDeltaIIAudioSource::State{};
AsusDeltaIIAudioSource::AsusDeltaIIAudioSource()=default;
AsusDeltaIIAudioSource::~AsusDeltaIIAudioSource()=default;
bool AsusDeltaIIAudioSource::Open(Time,std::string&){++audio_opens;state=std::make_unique<State>();return true;}
uint64_t AsusDeltaIIAudioSource::Generation()const{return state?77:0;}
void AsusDeltaIIAudioSource::Stop(){state.reset();}
AsusDeltaIIAudioGate::Sample AsusDeltaIIAudioSource::Read(){++audio_reads;return {77,Clock::now(),true,true,playing?1U:0U,false,playing?0.5f:0.0f,1.0f};}
auto wait(const std::shared_ptr<OpenRGBDeviceAwakeSave::Operation>& operation)
{
    const auto until=std::chrono::steady_clock::now()+2s;
    while(!operation->Read().done && std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(5ms);
    assert(operation->Read().done);return operation->Read();
}
int main()
{
    const char* path="HID#VID_0B05&PID_1AFA&MI_03&COL04#receiver";
    assert(IsAsusDeltaIICollection(0x0B05,0x1AFA,3,0xFF00,1,path));
    assert(!IsAsusDeltaIICollection(0x0B05,0x1943,3,0xFF00,1,path));
    assert(!IsAsusDeltaIICollection(0x0B05,0x1AFA,2,0xFF00,1,path));
    assert(!IsAsusDeltaIICollection(0x0B05,0x1AFA,3,0xFF13,1,path));
    assert(!IsAsusDeltaIICollection(0x0B05,0x1AFA,3,0xFF00,2,path));
    assert(!IsAsusDeltaIICollection(0x0B05,0x1AFA,3,0xFF00,1,"HID#VID_0B05&PID_1AFA&MI_03&COL03#receiver"));
    assert(!IsAsusDeltaIICollection(0x0B05,0x1AFA,3,0xFF00,1,nullptr));

    {
        RGBController_AsusDeltaII controller("receiver path","serial");
        assert(controller.GetDeviceType()==DEVICE_TYPE_HEADSET && controller.GetZoneCount()==1 && controller.GetLEDCount()==1);
        assert(controller.GetModeCount()==7);
        for(unsigned i=0;i<7;++i)assert(controller.GetModeFlags(i)&MODE_FLAG_MANUAL_SAVE);
        controller.SetAllColors(ToRGBColor(89,0,164));controller.SetActiveMode(1);controller.UpdateLEDs();
        std::this_thread::sleep_for(30ms);
        assert(io.opens==0 && io.writes==0 && audio_opens==0 && audio_reads==0);
        auto cancelled=controller.PrepareAwakeSave();cancelled->Cancel();cancelled->Confirm();
        assert(cancelled->Read().done && !cancelled->Read().success && io.opens==0);
        auto unavailableOp=controller.PrepareAwakeSave();unavailableOp->Confirm();auto unavailable=wait(unavailableOp);assert(!unavailable.success && !unavailable.message.empty());
        assert(io.opens==0 && audio_opens==0); // ASUS cannot fall back to USB.
        controller.SetDeviceSpecificConfiguration({{"rgb_backend","Direct receiver"}});
        controller.SaveMode(); // Raw SDK/CLI Save is not an awake confirmation.
        std::this_thread::sleep_for(20ms);assert(io.opens==0);
        auto appliedOp=controller.PrepareAwakeSave();appliedOp->Confirm();auto applied=wait(appliedOp);assert(applied.success);
        assert(io.opens==1 && io.writes==3 && audio_opens==0);
        appliedOp->Confirm();std::this_thread::sleep_for(20ms);assert(io.writes==3);
        controller.SetActiveMode(0);controller.SetAllColors(ToRGBColor(1,2,3));
        auto softwareOp=controller.PrepareAwakeSave();softwareOp->Confirm();auto software=wait(softwareOp);assert(software.success);
        assert(audio_opens==1 && io.writes==7);
        const auto setup=io.writes.load();
        for(int i=0;i<10;++i){controller.SetAllColors(ToRGBColor(i,0,0));controller.UpdateLEDs();std::this_thread::sleep_for(10ms);}
        assert(io.writes==setup); // No continuous audio settling yet.
        auto animate=[&](int milliseconds)
        {
            const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);unsigned i=0;
            while(std::chrono::steady_clock::now()<until)
            {controller.SetAllColors(ToRGBColor(i++%255,2,3));controller.UpdateLEDs();std::this_thread::sleep_for(25ms);}
        };
        animate(3300);assert(io.writes==setup+1); // 3s settling then bounded 1Hz output.
        playing=false;animate(1200);const auto paused=io.writes.load();animate(500);assert(io.writes==paused);
        playing=true;animate(3300);assert(io.writes==paused+1 && audio_opens==1 && io.opens==2);
        const auto beforeBackend=io.writes.load();
        controller.SetDeviceSpecificConfiguration({{"rgb_backend","ASUS integration"}});
        std::this_thread::sleep_for(30ms);
        assert(io.writes==beforeBackend); // Backend edit stops without cleanup writes.
    }
    std::shared_ptr<OpenRGBDeviceAwakeSave::Operation> removed;
    {RGBController_AsusDeltaII controller("receiver","serial");removed=controller.PrepareAwakeSave();}
    removed->Confirm();assert(removed->Read().done && !removed->Read().success);
    const auto before=io.writes.load();
    // Immediate destruction exercises initial scheduler flags and COM-owner
    // close-job handoff without introducing a dedicated worker or USB reader.
    for(int i=0;i<20;++i){RGBController_AsusDeltaII controller("receiver","serial");}
    assert(io.writes==before);
    std::cout<<"Native Delta II: real worker, seven modes/shared zone, staged-only startup/config/SDK-save, explicit awake actions, no fallback and orderly teardown passed\n";
}