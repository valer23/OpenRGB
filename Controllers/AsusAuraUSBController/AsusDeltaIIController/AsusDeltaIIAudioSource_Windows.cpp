/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "AsusDeltaIIAudioSource_Windows.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cwctype>

namespace
{
// MinGW's endpointvolume.h forward-declares this interface. Keep the published
// four-method Windows SDK ABI local to this source; no vendor/SDK headers change.
struct AudioMeter : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32,float*)=0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD*)=0;
};
const IID MeterId={0xc02216f6,0x8c67,0x4b5b,{0x9d,0x00,0xd0,0x08,0xe7,0x3e,0x00,0x64}};
template<class T> struct Com
{
    T* value=nullptr;
    ~Com(){if(value)value->Release();}
    Com()=default;Com(const Com&)=delete;Com& operator=(const Com&)=delete;
    T** Out(){return &value;}
    T* operator->()const{return value;}
};
std::string GuidString(const GUID& value)
{
    const GUID empty{};
    if(IsEqualGUID(empty,value))return {};
    wchar_t wide[40]{};
    if(!StringFromGUID2(value,wide,40))return {};
    std::string result;
    for(const auto* p=wide;*p;++p)result+=static_cast<char>(*p);
    return result;
}
std::string Utf8(const wchar_t* text)
{
    if(!text)return {};
    const int length=static_cast<int>(wcslen(text));
    const int count=WideCharToMultiByte(CP_UTF8,0,text,length,nullptr,0,nullptr,nullptr);
    std::string result(count,'\0');
    if(count)WideCharToMultiByte(CP_UTF8,0,text,length,result.data(),count,nullptr,nullptr);
    return result;
}
std::string ContainerProperty(IPropertyStore* store)
{
    const PROPERTYKEY key{DEVPKEY_Device_ContainerId.fmtid,DEVPKEY_Device_ContainerId.pid};
    PROPVARIANT value;PropVariantInit(&value);std::string result;
    if(SUCCEEDED(store->GetValue(key,&value)))
    {
        if(value.vt==VT_CLSID && value.puuid)result=GuidString(*value.puuid);
        else if(value.vt==VT_LPWSTR && value.pwszVal)
        {
            GUID parsed{};
            if(SUCCEEDED(CLSIDFromString(value.pwszVal,&parsed)))result=GuidString(parsed);
        }
    }
    PropVariantClear(&value);return result;
}
std::string ReceiverContainer()
{
    // PnP metadata only. Never activate a HID collection to discover identity.
    const auto devices=SetupDiGetClassDevsW(nullptr,L"USB",nullptr,DIGCF_PRESENT|DIGCF_ALLCLASSES);
    if(devices==INVALID_HANDLE_VALUE)return {};
    unsigned matches=0;std::string result;
    for(DWORD i=0;i<4096;++i)
    {
        SP_DEVINFO_DATA info{};info.cbSize=sizeof(info);
        if(!SetupDiEnumDeviceInfo(devices,i,&info))break;
        wchar_t id[1024]{};
        if(!SetupDiGetDeviceInstanceIdW(devices,&info,id,1024,nullptr))continue;
        std::wstring instance=id;
        std::transform(instance.begin(),instance.end(),instance.begin(),[](wchar_t c){return std::towupper(c);});
        if(instance.rfind(L"USB\\VID_0B05&PID_1AFA\\",0)!=0)continue;
        ++matches;GUID value{};DEVPROPTYPE type=0;DWORD bytes=0;
        if(SetupDiGetDevicePropertyW(devices,&info,&DEVPKEY_Device_ContainerId,&type,
            reinterpret_cast<BYTE*>(&value),sizeof(value),&bytes,0) &&
            type==DEVPROP_TYPE_GUID && bytes==sizeof(value))result=GuidString(value);
    }
    SetupDiDestroyDeviceInfoList(devices);
    return matches==1 ? result : std::string{};
}
AsusDeltaIIAudioSource::Endpoint Describe(IMMDevice* device)
{
    AsusDeltaIIAudioSource::Endpoint description{};
    LPWSTR id=nullptr;DWORD status=0;EDataFlow flow=eAll;
    Com<IMMEndpoint> endpoint;Com<IPropertyStore> store;
    if(FAILED(device->GetId(&id)))return description;
    description.id=Utf8(id);CoTaskMemFree(id);
    if(FAILED(device->GetState(&status)) || FAILED(device->OpenPropertyStore(STGM_READ,store.Out())) ||
       FAILED(device->QueryInterface(__uuidof(IMMEndpoint),reinterpret_cast<void**>(endpoint.Out()))) ||
       FAILED(endpoint->GetDataFlow(&flow)))return {};
    description.container=ContainerProperty(store.value);
    description.render=flow==eRender;description.active=status==DEVICE_STATE_ACTIVE;
    return description;
}
std::atomic<uint64_t> next_generation{0};
}
struct AsusDeltaIIAudioSource::State
{
    bool com_initialized=false;
    uint64_t generation=0;
    std::string endpoint_id,container;
    Com<IMMDeviceEnumerator> enumerator;
    Com<IMMDevice> endpoint;
    Com<IAudioSessionManager2> manager;
    Com<AudioMeter> meter;
    Com<IAudioEndpointVolume> volume;
};
AsusDeltaIIAudioSource::AsusDeltaIIAudioSource()=default;
AsusDeltaIIAudioSource::~AsusDeltaIIAudioSource(){Stop();}
uint64_t AsusDeltaIIAudioSource::Generation()const{return state ? state->generation : 0;}
void AsusDeltaIIAudioSource::Stop()
{
    const bool uninitialize=state && state->com_initialized;
    state.reset(); // Release interfaces before balancing CoInitializeEx.
    if(uninitialize)CoUninitialize();
}
bool AsusDeltaIIAudioSource::Open(Time confirmed_awake,std::string& error)
{
    error.clear();const auto now=Clock::now();
    if(confirmed_awake>now || now-confirmed_awake>=std::chrono::seconds(5))
    {error="Fresh explicit Delta II awake confirmation required for audio observation";return false;}
    Stop();state=std::make_unique<State>();
    auto fail=[&](const char* reason){error=reason;Stop();return false;};
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return fail("Audio observer COM apartment unavailable");
    state->com_initialized=true;
    // Locals holding COM interfaces must leave scope before Stop balances COM.
    auto initialize=[&]() -> bool
    {
        auto fail=[&](const char* reason){error=reason;return false;};
        state->container=ReceiverContainer();
        if(state->container.empty())return fail("Unique Delta II receiver container unavailable");
        if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,
            __uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(state->enumerator.Out()))))
            return fail("Windows playback endpoint enumeration unavailable");
        Com<IMMDeviceCollection> devices;
        if(FAILED(state->enumerator->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,devices.Out())))
            return fail("Active playback endpoints unavailable");
        UINT count=0;
        if(FAILED(devices->GetCount(&count)) || count>4096)return fail("Playback endpoint count unavailable");
        std::vector<Endpoint> descriptions;
        for(UINT i=0;i<count;++i)
        {
            Com<IMMDevice> device;
            if(FAILED(devices->Item(i,device.Out())))return fail("Playback endpoint metadata unavailable");
            descriptions.push_back(Describe(device.value));
        }
        state->endpoint_id=SelectEndpoint(state->container,descriptions);
        if(state->endpoint_id.empty())return fail("Unique Delta II playback endpoint unavailable");
        for(UINT i=0;i<count;++i)
            if(descriptions[i].id==state->endpoint_id)
            {if(FAILED(devices->Item(i,state->endpoint.Out())))return fail("Selected playback endpoint unavailable");break;}
        const auto selected=Describe(state->endpoint.value);
        if(SelectEndpoint(state->container,{selected})!=state->endpoint_id || ReceiverContainer()!=state->container)
            return fail("Delta II playback identity changed during selection");
        if(Clock::now()-confirmed_awake>=std::chrono::seconds(5))return fail("Delta II awake confirmation expired before audio activation");
        // Only the exact render endpoint is activated; no microphone, playback or
        // recording stream, HID handle, volume setter or device command is created.
        if(FAILED(state->endpoint->Activate(__uuidof(IAudioSessionManager2),CLSCTX_INPROC_SERVER,nullptr,
             reinterpret_cast<void**>(state->manager.Out()))) ||
           FAILED(state->endpoint->Activate(MeterId,CLSCTX_INPROC_SERVER,nullptr,reinterpret_cast<void**>(state->meter.Out()))) ||
           FAILED(state->endpoint->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_INPROC_SERVER,nullptr,
             reinterpret_cast<void**>(state->volume.Out()))))return fail("Read-only playback observation unavailable");
        DWORD meter_support=0,volume_support=0;
        if(FAILED(state->meter->QueryHardwareSupport(&meter_support)) || (meter_support&ENDPOINT_HARDWARE_SUPPORT_METER))
            return fail("Software-only Delta II peak meter required");
        if(FAILED(state->volume->QueryHardwareSupport(&volume_support)))return fail("Playback volume capabilities unavailable");
        if(Clock::now()-confirmed_awake>=std::chrono::seconds(5))return fail("Delta II awake confirmation expired during audio setup");
        return true;
    };
    if(!initialize()){Stop();return false;}
    do {state->generation=++next_generation;} while(!state->generation);
    return true;
}
AsusDeltaIIAudioGate::Sample AsusDeltaIIAudioSource::Read()
{
    AsusDeltaIIAudioGate::Sample reading{Generation(),Clock::now(),false,false,0,true,0,0};
    if(!state)return reading;
    const auto endpoint=Describe(state->endpoint.value);
    if(SelectEndpoint(state->container,{endpoint})!=state->endpoint_id || ReceiverContainer()!=state->container)
    {Stop();return reading;}
    reading.render_matches_receiver=true;
    auto collect=[&]()
    {
        Com<IAudioSessionEnumerator> sessions;int count=0;
        bool valid=SUCCEEDED(state->manager->GetSessionEnumerator(sessions.Out())) &&
            SUCCEEDED(sessions->GetCount(&count)) && count>=0 && count<=4096;
        for(int i=0;valid && i<count;++i)
        {
            Com<IAudioSessionControl> session;AudioSessionState status=AudioSessionStateExpired;
            valid=SUCCEEDED(sessions->GetSession(i,session.Out())) && SUCCEEDED(session->GetState(&status));
            if(valid && status==AudioSessionStateActive)++reading.active_sessions;
        }
        BOOL muted=TRUE;
        if(valid)valid=SUCCEEDED(state->meter->GetPeakValue(&reading.peak)) &&
            SUCCEEDED(state->volume->GetMute(&muted)) && SUCCEEDED(state->volume->GetMasterVolumeLevelScalar(&reading.volume)) &&
            std::isfinite(reading.peak) && reading.peak>=0 && reading.peak<=1 &&
            std::isfinite(reading.volume) && reading.volume>=0 && reading.volume<=1;
        reading.muted=muted!=FALSE;return valid;
    };
    reading.available=collect();
    if(!reading.available)Stop();
    return reading;
}