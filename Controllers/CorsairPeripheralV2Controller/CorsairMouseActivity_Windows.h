/*---------------------------------------------------------*\
| CorsairMouseActivity_Windows.h                            |
|                                                           |
| Windows Corsair wireless mouse activity timestamps        |
|                                                           |
| valer23                                     18 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#pragma once
#ifdef _WIN32
#include <windows.h>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Shares one mouse-only Raw Input registration. No coordinates, buttons or
// keystrokes are stored; only the time of the last matching mouse input.
class CorsairMouseActivity
{
public:
    static std::shared_ptr<CorsairMouseActivity> Acquire()
    {
        static std::mutex mutex;
        static std::weak_ptr<CorsairMouseActivity> existing;
        std::lock_guard<std::mutex> guard(mutex);
        std::shared_ptr<CorsairMouseActivity> result = existing.lock();
        if(!result)
        {
            result.reset(new CorsairMouseActivity);
            existing = result;
        }
        return result;
    }

    ~CorsairMouseActivity()
    {
        HWND handle = window.load();
        if(handle) PostMessageW(handle, WM_CLOSE, 0, 0);
        if(worker.joinable()) worker.join();
    }

    static unsigned long long Now()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    bool IdleMilliseconds(unsigned long long& age) const
    {
        HWND handle = window.load();
        if(!handle || !has_target.load() || MouseRegistration() != handle) return false;
        unsigned long long last = last_activity.load();
        unsigned long long now = Now();
        age = now >= last ? now - last : 0;
        return true;
    }

    bool Ready() const { return window.load() != nullptr; }

    static bool Matches(std::wstring path)
    {
        for(wchar_t& character : path) character = std::towupper(character);
        return path.find(L"VID_1B1C&PID_1BA6") != std::wstring::npos ||
               path.find(L"VID_1B1C&PID_1B66") != std::wstring::npos ||
               path.find(L"VID_1B1C&PID_1B65") != std::wstring::npos;
    }

private:
    std::thread worker;
    std::atomic<HWND> window{nullptr};
    std::atomic<bool> has_target{false};
    std::atomic<unsigned long long> last_activity{Now()};
    std::map<HANDLE, bool> devices; // Only accessed by the window thread.

    CorsairMouseActivity()
    {
        std::promise<bool> ready;
        std::future<bool> result = ready.get_future();
        worker = std::thread([this, &ready]() { Run(ready); });
        result.get();
    }

    static HWND MouseRegistration()
    {
        UINT count = 0;
        if(GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE)) == (UINT)-1 || count > 1024) return (HWND)-1;
        if(!count) return nullptr;
        std::vector<RAWINPUTDEVICE> registrations(count);
        UINT read = GetRegisteredRawInputDevices(registrations.data(), &count, sizeof(RAWINPUTDEVICE));
        if(read == (UINT)-1) return (HWND)-1;
        for(UINT i = 0; i < read; i++)
        {
            if(registrations[i].usUsagePage == 1 && registrations[i].usUsage == 2)
                return registrations[i].hwndTarget ? registrations[i].hwndTarget : (HWND)-1;
        }
        return nullptr;
    }

    bool IsTarget(HANDLE device)
    {
        std::map<HANDLE, bool>::iterator found = devices.find(device);
        if(found != devices.end()) return found->second;
        UINT length = 0;
        if(GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &length) == (UINT)-1 || !length || length > 4096) return false;
        std::vector<wchar_t> name(length + 1, 0);
        if(GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name.data(), &length) == (UINT)-1) return false;
        bool matches = Matches(std::wstring(name.data()));
        devices[device] = matches;
        return matches;
    }

    void RefreshTargets()
    {
        UINT count = 0;
        has_target.store(false);
        if(GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1 || count > 1024) return;
        if(!count) return;
        std::vector<RAWINPUTDEVICELIST> list(count);
        UINT read = GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST));
        if(read == (UINT)-1) return;
        for(UINT i = 0; i < read; i++)
        {
            if(list[i].dwType == RIM_TYPEMOUSE && IsTarget(list[i].hDevice)) has_target.store(true);
        }
    }

    void Read(HRAWINPUT handle)
    {
        RAWINPUT raw = {};
        UINT size = sizeof(raw);
        UINT read = GetRawInputData(handle, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
        if(read == (UINT)-1 || read < sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE) ||
           raw.header.dwType != RIM_TYPEMOUSE || !IsTarget(raw.header.hDevice)) return;
        if(raw.data.mouse.lLastX || raw.data.mouse.lLastY || raw.data.mouse.usButtonFlags)
            last_activity.store(Now());
    }

    static LRESULT CALLBACK Procedure(HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if(message == WM_NCCREATE)
        {
            CREATESTRUCTW* create = (CREATESTRUCTW*)lparam;
            SetWindowLongPtrW(handle, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
        }
        CorsairMouseActivity* self = (CorsairMouseActivity*)GetWindowLongPtrW(handle, GWLP_USERDATA);
        if(message == WM_INPUT && self) self->Read((HRAWINPUT)lparam);
        if(message == WM_INPUT_DEVICE_CHANGE && self)
        {
            self->devices.erase((HANDLE)lparam);
            self->RefreshTargets();
        }
        if(message == WM_CLOSE) { DestroyWindow(handle); return 0; }
        if(message == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(handle, message, wparam, lparam);
    }

    void Run(std::promise<bool>& ready)
    {
        // Do not replace a mouse listener already owned by the application/plugin.
        if(MouseRegistration() != nullptr) { ready.set_value(false); return; }
        const wchar_t* name = L"OpenRGB_Corsair_Mouse_Idle";
        WNDCLASSW klass = {};
        klass.lpfnWndProc = Procedure;
        klass.hInstance = GetModuleHandleW(nullptr);
        klass.lpszClassName = name;
        if(!RegisterClassW(&klass)) { ready.set_value(false); return; }
        HWND handle = CreateWindowExW(0, name, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, klass.hInstance, this);
        RAWINPUTDEVICE registration = {1, 2, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, handle};
        if(!handle || !RegisterRawInputDevices(&registration, 1, sizeof(registration)))
        {
            if(handle) DestroyWindow(handle);
            UnregisterClassW(name, klass.hInstance);
            ready.set_value(false);
            return;
        }
        RefreshTargets();
        window.store(handle);
        ready.set_value(true);
        MSG message = {};
        while(GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if(MouseRegistration() == handle)
        {
            RAWINPUTDEVICE remove = {1, 2, RIDEV_REMOVE, nullptr};
            RegisterRawInputDevices(&remove, 1, sizeof(remove));
        }
        window.store(nullptr);
        if(IsWindow(handle)) DestroyWindow(handle);
        UnregisterClassW(name, klass.hInstance);
    }
};
#endif
