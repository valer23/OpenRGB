/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#ifdef _WIN32
#include <windows.h>
#include <cstring>
#include <mutex>
#endif

class EffectTimerResolution
{
public:
    EffectTimerResolution()
    {
#ifdef _WIN32
        module = LoadLibraryW(L"winmm.dll");
        if(module)
        {
            auto begin = LoadPeriodFunction(module, "timeBeginPeriod");
            end = LoadPeriodFunction(module, "timeEndPeriod");
            active = begin && end && begin(1) == 0;
            if(active)
            {
                std::lock_guard<std::mutex> guard(policy_mutex);
                if(users++ == 0) AcquireBackgroundTiming();
            }
        }
#endif
    }
    ~EffectTimerResolution()
    {
#ifdef _WIN32
        if(active)
        {
            std::lock_guard<std::mutex> guard(policy_mutex);
            if(--users == 0) RestoreBackgroundTiming();
            end(1);
        }
        if(module) FreeLibrary(module);
#endif
    }
    bool IsActive() const
    {
#ifdef _WIN32
        return active;
#else
        return false;
#endif
    }
    EffectTimerResolution(const EffectTimerResolution&) = delete;
    EffectTimerResolution& operator=(const EffectTimerResolution&) = delete;
private:
#ifdef _WIN32
    using PeriodFunction = UINT (WINAPI*)(UINT);
    static PeriodFunction LoadPeriodFunction(HMODULE module, const char* name)
    {
        FARPROC address = GetProcAddress(module, name);
        PeriodFunction function = nullptr;
        static_assert(sizeof(function) == sizeof(address), "Windows function pointer size mismatch");
        std::memcpy(&function, &address, sizeof(function));
        return function;
    }
    using ProcessFunction = BOOL (WINAPI*)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
    static ProcessFunction LoadProcessFunction(const char* name)
    {
        FARPROC address = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), name);
        ProcessFunction function = nullptr;
        static_assert(sizeof(function) == sizeof(address), "Windows function pointer size mismatch");
        std::memcpy(&function, &address, sizeof(function));
        return function;
    }
    static void AcquireBackgroundTiming()
    {
        get_process = LoadProcessFunction("GetProcessInformation");
        set_process = LoadProcessFunction("SetProcessInformation");
        saved_policy = {};
        saved_policy.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        policy_changed = false;
        if(get_process && set_process && get_process(GetCurrentProcess(), ProcessPowerThrottling, &saved_policy, sizeof(saved_policy)))
        {
            auto policy = saved_policy;
            policy.ControlMask |= PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
            policy.StateMask &= ~PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
            policy_changed = set_process(GetCurrentProcess(), ProcessPowerThrottling, &policy, sizeof(policy));
        }
    }
    static void RestoreBackgroundTiming()
    {
        if(!policy_changed) return;
        auto policy = saved_policy;
        if(get_process(GetCurrentProcess(), ProcessPowerThrottling, &policy, sizeof(policy)))
        {
            const ULONG mask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
            policy.ControlMask = (policy.ControlMask & ~mask) | (saved_policy.ControlMask & mask);
            policy.StateMask = (policy.StateMask & ~mask) | (saved_policy.StateMask & mask);
        }
        set_process(GetCurrentProcess(), ProcessPowerThrottling, &policy, sizeof(policy));
        policy_changed = false;
    }
    inline static std::mutex policy_mutex;
    inline static unsigned int users = 0;
    inline static ProcessFunction get_process = nullptr;
    inline static ProcessFunction set_process = nullptr;
    inline static PROCESS_POWER_THROTTLING_STATE saved_policy = {};
    inline static bool policy_changed = false;
    HMODULE module = nullptr;
    PeriodFunction end = nullptr;
    bool active = false;
#endif
};
