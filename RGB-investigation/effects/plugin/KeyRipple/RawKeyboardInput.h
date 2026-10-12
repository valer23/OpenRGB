/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#ifdef _WIN32
#include <windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class RawKeyboardInput
{
public:
    using Clock = std::chrono::steady_clock;
    struct Press { unsigned int scan; Clock::time_point time; };
    ~RawKeyboardInput() { Stop(); }
    bool Start()
    {
        if(worker.joinable()) return window.load() != nullptr;
        ownership = std::unique_lock<std::mutex>(registration_mutex, std::try_to_lock);
        if(!ownership.owns_lock()) return false;
        std::promise<bool> ready;
        auto result = ready.get_future();
        worker = std::thread([this, &ready]() { Run(ready); });
        const bool success = result.get();
        if(!success) Stop();
        return success;
    }
    void Stop()
    {
        if(HWND handle = window.load()) PostMessageW(handle, WM_CLOSE, 0, 0);
        if(worker.joinable()) worker.join();
        std::lock_guard<std::mutex> guard(queue_mutex);
        queue.clear();
        if(ownership.owns_lock()) ownership.unlock();
    }
    std::vector<Press> Drain()
    {
        std::lock_guard<std::mutex> guard(queue_mutex);
        std::vector<Press> result(queue.begin(), queue.end());
        queue.clear();
        return result;
    }
private:
    inline static std::mutex registration_mutex;
    std::unique_lock<std::mutex> ownership;
    std::thread worker;
    std::atomic<HWND> window{nullptr};
    std::mutex queue_mutex;
    std::deque<Press> queue;
    // These maps are only accessed on the input thread, reset on each start.
    std::map<HANDLE, bool> devices;
    std::map<HANDLE, std::array<bool, 512>> pressed;

    bool IsLightMount(HANDLE device)
    {
        auto found = devices.find(device);
        if(found != devices.end()) return found->second;
        UINT length = 0;
        if(GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &length) == static_cast<UINT>(-1) || !length || length > 4096) return false;
        std::vector<wchar_t> name(length + 1, 0);
        if(GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name.data(), &length) == static_cast<UINT>(-1)) return false;
        std::wstring path(name.data());
        for(auto& character : path) character = std::towupper(character);
        const bool matches = path.find(L"VID_373F&PID_0002") != std::wstring::npos;
        devices.emplace(device, matches);
        return matches;
    }
    void Read(HRAWINPUT handle)
    {
        RAWINPUT raw{};
        UINT size = sizeof(raw);
        const UINT read = GetRawInputData(handle, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
        if(read == static_cast<UINT>(-1) || read < sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD) || raw.header.dwType != RIM_TYPEKEYBOARD || !IsLightMount(raw.header.hDevice)) return;
        const auto& key = raw.data.keyboard;
        if(key.MakeCode == 0 || key.MakeCode >= 256 || (key.Flags & RI_KEY_E1)) return;
        const unsigned int scan = key.MakeCode | ((key.Flags & RI_KEY_E0) ? 0x100 : 0);
        auto& state = pressed[raw.header.hDevice];
        if(key.Flags & RI_KEY_BREAK) { state[scan] = false; return; }
        if(state[scan]) return; // held-key repeat is not another physical press.
        state[scan] = true;
        std::lock_guard<std::mutex> guard(queue_mutex);
        if(queue.size() == 64) queue.pop_front();
        queue.push_back({scan, Clock::now()});
    }
    static LRESULT CALLBACK Procedure(HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if(message == WM_NCCREATE)
        {
            const auto* creation = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
        }
        auto* self = reinterpret_cast<RawKeyboardInput*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
        if(message == WM_INPUT && self) self->Read(reinterpret_cast<HRAWINPUT>(lparam));
        if(message == WM_CLOSE) { DestroyWindow(handle); return 0; }
        if(message == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(handle, message, wparam, lparam);
    }
    void Run(std::promise<bool>& ready)
    {
        devices.clear(); pressed.clear();
        const wchar_t* name = L"OpenRGB_Custom_KeyRipple_Input";
        WNDCLASSW klass{};
        klass.lpfnWndProc = Procedure;
        klass.hInstance = GetModuleHandleW(nullptr);
        klass.lpszClassName = name;
        if(!RegisterClassW(&klass)) { ready.set_value(false); return; }
        HWND handle = CreateWindowExW(0, name, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, klass.hInstance, this);
        RAWINPUTDEVICE registration{0x01, 0x06, RIDEV_INPUTSINK, handle};
        if(!handle || !RegisterRawInputDevices(&registration, 1, sizeof(registration)))
        {
            if(handle) DestroyWindow(handle);
            UnregisterClassW(name, klass.hInstance);
            ready.set_value(false);
            return;
        }
        window.store(handle);
        ready.set_value(true);
        MSG message{};
        while(GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        if(IsWindow(handle)) DestroyWindow(handle);
        RAWINPUTDEVICE remove{0x01, 0x06, RIDEV_REMOVE, nullptr};
        RegisterRawInputDevices(&remove, 1, sizeof(remove));
        window.store(nullptr);
        UnregisterClassW(name, klass.hInstance);
    }
};
#endif
