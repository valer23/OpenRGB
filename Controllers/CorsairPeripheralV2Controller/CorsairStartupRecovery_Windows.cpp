/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cwctype>
#include <future>
#include <map>
#include <thread>
#include <vector>
#include "CorsairStartupRecovery_Windows.h"
#include "CorsairStartupRecoveryPolicy.h"
#include "DetectionManager.h"
#include "LogManager.h"

namespace
{
using Clock=std::chrono::steady_clock;
std::wstring Wide(const std::string& value)
{
    const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    if(size<=0)return {};
    std::wstring result(size,0);
    MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),size);
    return result;
}
std::string Container(const std::wstring& path)
{
    // Device-interface properties are Windows metadata. No HID handle or device
    // request is opened; the ordinary input driver remains the only reader.
    const auto set=SetupDiCreateDeviceInfoList(nullptr,nullptr);
    if(set==INVALID_HANDLE_VALUE)return {};
    SP_DEVICE_INTERFACE_DATA interface_data{};interface_data.cbSize=sizeof(interface_data);
    SP_DEVINFO_DATA device{};device.cbSize=sizeof(device);
    std::string result;
    if(SetupDiOpenDeviceInterfaceW(set,path.c_str(),0,&interface_data))
    {
        DWORD needed=0;
        SetupDiGetDeviceInterfaceDetailW(set,&interface_data,nullptr,0,&needed,nullptr);
        if(needed>=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) && needed<65536)
        {
            std::vector<unsigned char> storage(needed);
            auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
            detail->cbSize=sizeof(*detail);
            if(SetupDiGetDeviceInterfaceDetailW(set,&interface_data,detail,needed,nullptr,&device))
            {
                GUID value{};DEVPROPTYPE type=0;DWORD count=0;
                if(SetupDiGetDevicePropertyW(set,&device,&DEVPKEY_Device_ContainerId,&type,
                    reinterpret_cast<BYTE*>(&value),sizeof(value),&count,0) && type==DEVPROP_TYPE_GUID && count==sizeof(value))
                {
                    const GUID empty{};
                    if(std::memcmp(&empty,&value,sizeof(value)))result.assign(reinterpret_cast<const char*>(&value),sizeof(value));
                }
            }
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return result;
}
bool MouseRegistrationAvailable(HWND ours=nullptr)
{
    UINT count=0;
    if(GetRegisteredRawInputDevices(nullptr,&count,sizeof(RAWINPUTDEVICE))==static_cast<UINT>(-1) || count>4096)return false;
    std::vector<RAWINPUTDEVICE> devices(count);
    if(count && GetRegisteredRawInputDevices(devices.data(),&count,sizeof(RAWINPUTDEVICE))==static_cast<UINT>(-1))return false;
    bool found_ours=false;
    for(const auto& device:devices)
    {
        if(device.usUsagePage!=1)continue;
        if(device.usUsage==2 || (device.usUsage==0 && (device.dwFlags&RIDEV_PAGEONLY)))
        {
            if(!ours || device.usUsage!=2 || device.hwndTarget!=ours)return false;
            found_ours=true;
        }
    }
    return !ours || found_ours;
}
class Recovery
{
public:
    ~Recovery(){Stop();}
    void Start(){std::lock_guard<std::mutex> lock(lifecycle);accepting=true;}
    void Stop()
    {
        std::lock_guard<std::mutex> lock(lifecycle);
        accepting=false;
        {std::lock_guard<std::mutex> guard(mutex);policy.Clear();}
        if(const auto handle=window.load())PostMessageW(handle,WM_CLOSE,0,0);
        if(worker.joinable())worker.join();
    }
    void Watch(const char* path)
    {
        if(!path)return;
        std::lock_guard<std::mutex> lock(lifecycle);
        if(!accepting)return;
        const std::string copied(path);
        const auto container=Container(Wide(copied));
        if(container.empty())
        {
            LOG_WARNING("[Corsair startup recovery] Receiver identity unavailable; use awake manual rescan");
            return;
        }
        {std::lock_guard<std::mutex> guard(mutex);policy.Watch(copied,container);}
        if(const auto handle=window.load()){PostMessageW(handle,WM_APP+1,0,0);return;}
        if(worker.joinable())worker.join();
        auto ready=std::make_shared<std::promise<bool>>();auto result=ready->get_future();
        worker=std::thread([this,ready](){Run(ready);});
        if(!result.get())LOG_WARNING("[Corsair startup recovery] Input observer unavailable; awake manual rescan remains available");
    }
    void Forget(const char* path)
    {
        if(path)
        {
            const auto container=Container(Wide(path));
            {std::lock_guard<std::mutex> lock(mutex);policy.Forget(path);if(!container.empty())policy.ForgetReceiver(container);}
            if(const auto handle=window.load())PostMessageW(handle,WM_APP+1,0,0);
        }
    }
private:
    std::mutex lifecycle,mutex;
    CorsairStartupRecoveryPolicy policy;
    std::thread worker;
    std::atomic<HWND> window{nullptr};
    bool accepting=false; // lifecycle lock; queued work is fenced by policy epoch.
    std::map<HANDLE,std::string> containers; // message-thread only.
    void Read(HWND handle,HRAWINPUT input)
    {
        const auto age=CorsairMouseInputAge(GetTickCount(),static_cast<DWORD>(GetMessageTime()));
        if(age>250)return; // A delayed message is not fresh activity.
        const auto observed=Clock::now()-std::chrono::milliseconds(age);
        {std::lock_guard<std::mutex> lock(mutex);if(policy.Empty())return;}
        if(!MouseRegistrationAvailable(handle)){PostMessageW(handle,WM_CLOSE,0,0);return;}
        RAWINPUT raw{};UINT size=sizeof(raw);
        const auto read=GetRawInputData(input,RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER));
        if(read==static_cast<UINT>(-1) || read<sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE) ||
           raw.header.dwType!=RIM_TYPEMOUSE || !raw.header.hDevice ||
           !CorsairMouseInputMeaningful(raw.data.mouse.lLastX,raw.data.mouse.lLastY,raw.data.mouse.usButtonFlags))return;
        auto found=containers.find(raw.header.hDevice);
        if(found==containers.end())
        {
            UINT length=0;
            if(GetRawInputDeviceInfoW(raw.header.hDevice,RIDI_DEVICENAME,nullptr,&length)==static_cast<UINT>(-1) || !length || length>4096)return;
            std::vector<wchar_t> name(length+1,0);
            if(GetRawInputDeviceInfoW(raw.header.hDevice,RIDI_DEVICENAME,name.data(),&length)==static_cast<UINT>(-1))return;
            std::wstring path(name.data());std::transform(path.begin(),path.end(),path.begin(),[](wchar_t c){return std::towupper(c);});
            std::string container;
            if(path.find(L"HID#VID_1B1C&PID_1BA6&MI_")!=std::wstring::npos)container=Container(path);
            found=containers.emplace(raw.header.hDevice,std::move(container)).first;
        }
        if(found->second.empty())return;
        std::optional<CorsairStartupRecoveryPolicy::Request> request;
        {std::lock_guard<std::mutex> lock(mutex);request=policy.Input(found->second,observed,Clock::now());}
        if(!request)return;
        auto current=[this,copy=*request]()
        {
            {std::lock_guard<std::mutex> lock(mutex);if(!policy.Current(copy,Clock::now()))return false;}
            // Paths can be reused after replug. Validate the CURRENT control
            // interface's physical identity, then recheck age/cleanup epoch.
            if(Container(Wide(copy.path))!=copy.container)return false;
            std::lock_guard<std::mutex> lock(mutex);return policy.Current(copy,Clock::now());
        };
        auto finished=[this,copy=*request](bool recovered)
        {
            {std::lock_guard<std::mutex> lock(mutex);policy.Finished(copy,recovered);}
            if(const auto handle=window.load())PostMessageW(handle,WM_APP+1,0,0);
        };
        if(!DetectionManager::get()->TryDetectCorsairFromInput(request->path,request->observed,current,finished))finished(false);
    }
    static LRESULT CALLBACK Procedure(HWND handle,UINT message,WPARAM wparam,LPARAM lparam)
    {
        if(message==WM_NCCREATE)
        {
            const auto* create=reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(handle,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto* self=reinterpret_cast<Recovery*>(GetWindowLongPtrW(handle,GWLP_USERDATA));
        if(message==WM_INPUT && self)self->Read(handle,reinterpret_cast<HRAWINPUT>(lparam));
        if(message==WM_INPUT_DEVICE_CHANGE && self)self->containers.clear();
        if(message==WM_APP+1 && self)
        {
            bool empty;
            {std::lock_guard<std::mutex> lock(self->mutex);empty=self->policy.Empty();}
            if(empty && MouseRegistrationAvailable(handle))
            {
                RAWINPUTDEVICE remove{1,2,RIDEV_REMOVE,nullptr};RegisterRawInputDevices(&remove,1,sizeof(remove));
            }
            else if(!empty && MouseRegistrationAvailable())
            {
                self->containers.clear();
                RAWINPUTDEVICE device{1,2,RIDEV_INPUTSINK|RIDEV_DEVNOTIFY,handle};RegisterRawInputDevices(&device,1,sizeof(device));
            }
            return 0;
        }
        if(message==WM_CLOSE){DestroyWindow(handle);return 0;}
        if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
        return DefWindowProcW(handle,message,wparam,lparam);
    }
    void Run(const std::shared_ptr<std::promise<bool>>& ready)
    {
        containers.clear();const wchar_t* name=L"OpenRGB_CorsairStartupRecovery";
        WNDCLASSW klass{};klass.lpfnWndProc=Procedure;klass.hInstance=GetModuleHandleW(nullptr);klass.lpszClassName=name;
        if(!MouseRegistrationAvailable() || !RegisterClassW(&klass)){ready->set_value(false);return;}
        HWND handle=CreateWindowExW(0,name,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,klass.hInstance,this);
        RAWINPUTDEVICE device{1,2,RIDEV_INPUTSINK|RIDEV_DEVNOTIFY,handle};
        if(!handle || !MouseRegistrationAvailable() || !RegisterRawInputDevices(&device,1,sizeof(device)))
        {
            if(handle)DestroyWindow(handle);
            UnregisterClassW(name,klass.hInstance);ready->set_value(false);return;
        }
        window=handle;ready->set_value(true);
        MSG message{};
        while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
        // Another component can replace a same-process registration. Never remove it.
        if(MouseRegistrationAvailable(handle))
        {
            RAWINPUTDEVICE remove{1,2,RIDEV_REMOVE,nullptr};RegisterRawInputDevices(&remove,1,sizeof(remove));
        }
        if(IsWindow(handle))DestroyWindow(handle);
        window=nullptr;containers.clear();UnregisterClassW(name,klass.hInstance);
    }
};
Recovery& Observer(){static Recovery observer;return observer;}
}
void StartCorsairStartupRecovery(){Observer().Start();}
void StopCorsairStartupRecovery(){Observer().Stop();}
void WatchMissingCorsairReceiver(const char* path){Observer().Watch(path);}
void ForgetMissingCorsairReceiver(const char* path){Observer().Forget(path);}
std::mutex& CorsairReceiverDetectionMutex(){static std::mutex mutex;return mutex;}
