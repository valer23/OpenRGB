/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "RGBController_AsusDeltaII_Windows.h"
#include "LogManager.h"
using namespace std::chrono_literals;

RGBController_AsusDeltaII::RGBController_AsusDeltaII(const std::string& path,const std::string& device_serial) :
    lifetime(std::make_shared<Lifetime>()),direct(GetAsusDeltaIIReceiver()),control(direct)
{
    name="ASUS ROG Delta II";vendor="ASUS";type=DEVICE_TYPE_HEADSET;
    description="Both side logos share one colour. Use Save To Device while the headset is awake. "
        "Direct animation follows audible playback on Windows. ASUS RGB integration is not available yet.";
    location="HID: "+path;serial=device_serial;version="Experimental Windows RGB";
    flags=CONTROLLER_FLAG_RESET_BEFORE_UPDATE;active_mode=0;
    const char* names[]={"Direct","Static","Breathing","Strobing","Color cycle","Rainbow","Off"};
    for(unsigned i=0;i<7;++i)
    {
        mode entry{};entry.name=names[i];entry.value=i;entry.flags=MODE_FLAG_MANUAL_SAVE;
        entry.brightness_min=0;entry.brightness_max=100;entry.brightness=50;
        if(i<4){entry.flags|=MODE_FLAG_HAS_PER_LED_COLOR;entry.color_mode=MODE_COLORS_PER_LED;}
        else entry.color_mode=MODE_COLORS_NONE;
        if(i!=6)entry.flags|=MODE_FLAG_HAS_BRIGHTNESS;
        modes.push_back(entry);
    }
    SetupZones();
    nlohmann::json settings;
    settings["schema"]["rgb_backend"]={
        {"title","RGB backend (experimental)"},{"type","string"},{"order",0},
        {"enum",{"ASUS integration","Direct receiver"}},
        {"description","ASUS RGB integration is not available yet. Direct receiver requires exclusive access and never stops services. "
            "Settings and profile loads only stage lighting. Use Save To Device and confirm the headset is awake to apply. "
            "Direct animation is limited to 1 Hz and pauses without audible playback."}};
    settings["configuration"]["rgb_backend"]="ASUS integration";
    configuration=settings.dump();lifetime->owner=this;
}
RGBController_AsusDeltaII::~RGBController_AsusDeltaII()
{
    {std::lock_guard<std::mutex> lock(lifetime->mutex);lifetime->owner=nullptr;}
    {
        std::lock_guard<std::mutex> lock(DeviceMutex);
        closing=true;CancelPending("Headset controller closed before applying lighting");ScheduleDeviceUpdate();
    }
    // COM and the retained receiver close on their owning existing device thread.
    {std::unique_lock<std::mutex> lock(close_mutex);close_ready.wait(lock,[this]{return closed;});}
    Shutdown();
}
void RGBController_AsusDeltaII::SetupZones()
{
    zones.clear();leds.clear();
    zone logos{};logos.name="Side logos (shared)";logos.type=ZONE_TYPE_SINGLE;
    logos.leds_min=1;logos.leds_max=1;logos.leds_count=1;zones.push_back(logos);
    led logo{};logo.name="Both side logos";leds.push_back(logo);SetupColors();
}
void RGBController_AsusDeltaII::ResizeZone(int,int){}
std::shared_ptr<OpenRGBDeviceAwakeSave::Operation> RGBController_AsusDeltaII::PrepareAwakeSave()
{
    auto operation=std::make_shared<Operation>(
        "Confirm that the Delta II is powered on and awake. Apply the selected lighting now?\n\n"
        "Direct animation follows audible playback and pauses during silence. "
        "Software using the receiver must release it before Direct receiver can connect.");
    std::weak_ptr<Lifetime> weak_lifetime=lifetime;
    std::weak_ptr<Operation> weak_operation=operation;
    operation->SetConfirmation([weak_lifetime,weak_operation]()
    {
        const auto confirmed=std::chrono::steady_clock::now();
        const auto operation=weak_operation.lock();if(!operation)return;
        const auto live=weak_lifetime.lock();
        if(!live){operation->Complete(false,"Headset is no longer available");return;}
        std::lock_guard<std::mutex> lock(live->mutex);
        if(!live->owner){operation->Complete(false,"Headset is no longer available");return;}
        live->owner->QueueAwake(operation,confirmed);
    });
    return operation;
}
void RGBController_AsusDeltaII::QueueAwake(const std::shared_ptr<Operation>& operation,Time confirmed)
{
    std::shared_lock<std::shared_mutex> access(AccessMutex);
    std::lock_guard<std::mutex> lock(DeviceMutex);
    if(closing || operation->Cancelled() || active_mode<0 || static_cast<size_t>(active_mode)>=modes.size())
    {operation->Complete(false,"Headset lighting is unavailable");return;}
    CancelPending("A newer headset lighting action replaced this request");
    const RGBColor colour=colors.empty()?0:colors[0];
    pending=Request{operation,confirmed,active_mode,modes[active_mode].brightness,
        {static_cast<unsigned char>(RGBGetRValue(colour)),static_cast<unsigned char>(RGBGetGValue(colour)),
         static_cast<unsigned char>(RGBGetBValue(colour))},backend};
    ScheduleDeviceUpdate();
}
void RGBController_AsusDeltaII::CancelPending(const std::string& reason)
{
    if(pending){pending->operation->Complete(false,reason);pending.reset();}
}
void RGBController_AsusDeltaII::StopOnWorker()
{
    control.Stop();audio.Stop();streaming=false;applied_mode=-1;last_audio={};
}
void RGBController_AsusDeltaII::Apply(Request request)
{
    StopOnWorker();
    auto finish=[&](bool success,const std::string& text)
    {
        if(!success)StopOnWorker();
        request.operation->Complete(success,text);
        if(success)LOG_INFO("[Delta II RGB] %s",text.c_str());
        else LOG_WARNING("[Delta II RGB] %s",text.c_str());
    };
    const auto now=std::chrono::steady_clock::now();
    if(request.operation->Cancelled() || request.confirmed>now || now-request.confirmed>=5s)
    {finish(false,"Awake confirmation expired; confirm again while the headset is awake");return;}
    control.SelectBackend(request.backend);
    if(request.backend!=Backend::DirectReceiver)
    {finish(false,"ASUS RGB integration is not available yet; no direct fallback was attempted");return;}
    std::string error;bool success=false;
    if(request.mode==0)
    {
        if(!audio.Open(request.confirmed,error)){finish(false,error);return;}
        if(request.operation->Cancelled()){finish(false,"Headset lighting operation cancelled");return;}
        control.SetAudioSource(audio.Generation(),true);
        success=control.BeginSoftware(request.brightness,request.colour,request.confirmed,error);
        streaming=success;
    }
    else if(request.mode==6)success=control.ApplyOff(request.confirmed,error);
    else success=control.ApplyBuiltin(request.mode,request.brightness,request.colour,request.confirmed,error);
    if(!success){finish(false,error);return;}
    applied_mode=request.mode;applied_brightness=request.brightness;
    finish(true,streaming ? "Direct RGB active. Animation follows audible playback and pauses during silence." : "Headset firmware lighting applied.");
}
void RGBController_AsusDeltaII::DeviceUpdateLEDs()
{
    if(closing)
    {
        StopOnWorker();
        {std::lock_guard<std::mutex> lock(close_mutex);closed=true;}
        close_ready.notify_one();return;
    }
    if(stop_requested){StopOnWorker();stop_requested=false;}
    if(pending){auto request=std::move(*pending);pending.reset();Apply(std::move(request));}
    if(!streaming || active_mode!=0)return;
    const auto now=std::chrono::steady_clock::now();
    if(last_audio==Time{} || now-last_audio>=100ms)
    {
        last_audio=now;const auto sample=audio.Read();
        control.ObserveAudio(sample,std::chrono::steady_clock::now());
        if(!sample.available || !sample.render_matches_receiver){StopOnWorker();return;}
    }
    const RGBColor colour=colors.empty()?0:colors[0];
    control.Offer({static_cast<unsigned char>(RGBGetRValue(colour)),static_cast<unsigned char>(RGBGetGValue(colour)),
        static_cast<unsigned char>(RGBGetBValue(colour))},std::chrono::steady_clock::now());
    std::string error;control.Pump(std::chrono::steady_clock::now(),error);
    if(!error.empty()){LOG_WARNING("[Delta II RGB] %s",error.c_str());StopOnWorker();}
}
void RGBController_AsusDeltaII::DeviceUpdateZoneLEDs(int){ScheduleDeviceUpdate();}
void RGBController_AsusDeltaII::DeviceUpdateSingleLED(int){ScheduleDeviceUpdate();}
void RGBController_AsusDeltaII::DeviceUpdateMode()
{
    if(active_mode<0 || static_cast<size_t>(active_mode)>=modes.size())
    {CancelPending("Invalid headset lighting mode");stop_requested=true;ScheduleDeviceUpdate();return;}
    if(pending && (pending->mode!=active_mode || pending->brightness!=modes[active_mode].brightness))
        CancelPending("Lighting selection changed; confirm the awake headset again");
    if(applied_mode!=active_mode || applied_brightness!=modes[active_mode].brightness)
    {stop_requested=true;ScheduleDeviceUpdate();}
}
void RGBController_AsusDeltaII::DeviceSaveMode()
{
    LOG_WARNING("[Delta II RGB] Use the local Save To Device confirmation while the headset is awake; no commands sent");
}
void RGBController_AsusDeltaII::DeviceUpdateDeviceSpecificConfiguration()
{
    Backend selected=Backend::AsusService;
    try
    {
        const auto settings=nlohmann::json::parse(configuration).at("configuration");
        if(settings.at("rgb_backend")=="Direct receiver")selected=Backend::DirectReceiver;
    }
    catch(...){}
    backend=selected;stop_requested=true;CancelPending("RGB backend changed; confirm the awake headset again");ScheduleDeviceUpdate();
}