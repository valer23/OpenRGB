/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "AsusDeltaIIBattery.h"
#include "BatteryProtocol.h"
#include "WindowsHidTransfer.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}
#include <algorithm>
#include <cwctype>
#include <vector>
#include <array>
#include <chrono>
#include <mutex>

namespace
{
using Clock = std::chrono::steady_clock;
std::mutex delta_mutex;
WindowsHidTransfer& DeltaTransfer()
{
    // Process lifetime: a defective driver may still own a cancelled operation
    // during shutdown. There is no background thread or plugin-owned callback.
    static auto* transfer = new WindowsHidTransfer(WindowsHidTransfer::Api{});
    return *transfer;
}
struct Handle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if(value != INVALID_HANDLE_VALUE && value != nullptr) { CloseHandle(value); } }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

// Metadata enumeration has no command writes or feature-report requests.
bool OpenExactDelta(Handle& handle, std::string& error)
{
    if(!DeltaTransfer().Ready())
    {
        error = "Previous Delta II I/O cancellation is pending; no new reader opened";
        return false;
    }
    GUID guid;
    HidD_GetHidGuid(&guid);
    auto devices = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if(devices == INVALID_HANDLE_VALUE) { error = "HID metadata unavailable"; return false; }
    std::vector<std::wstring> paths;
    for(DWORD i = 0; i < 4096; ++i)
    {
        SP_DEVICE_INTERFACE_DATA info = {};
        info.cbSize = sizeof(info);
        if(!SetupDiEnumDeviceInterfaces(devices, nullptr, &guid, i, &info)) { break; }
        DWORD bytes = 0;
        SetupDiGetDeviceInterfaceDetailW(devices, &info, nullptr, 0, &bytes, nullptr);
        if(bytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || bytes > 65536) { continue; }
        std::vector<unsigned char> storage(bytes);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(*detail);
        if(!SetupDiGetDeviceInterfaceDetailW(devices, &info, detail, bytes, nullptr, nullptr)) { continue; }
        std::wstring path = detail->DevicePath;
        std::transform(path.begin(), path.end(), path.begin(), [](wchar_t c) { return std::towlower(c); });
        if(path.find(L"vid_0b05&pid_1afa") != std::wstring::npos && path.find(L"mi_03&col04") != std::wstring::npos)
        {
            paths.push_back(detail->DevicePath);
        }
    }
    SetupDiDestroyDeviceInfoList(devices);
    if(paths.size() != 1)
    {
        error = paths.empty() ? "Exact Delta II vendor collection not present" : "Ambiguous Delta II receivers; no handle opened";
        return false;
    }
    // Deliberately no sharing. Never run alongside another control/event reader.
    handle.value = CreateFileW(paths[0].c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if(handle.value == INVALID_HANDLE_VALUE)
    {
        const auto windows_error = GetLastError();
        error = "Delta II vendor collection busy or unavailable (Windows error " + std::to_string(windows_error) +
            "); no competing reader opened";
        return false;
    }
    HIDD_ATTRIBUTES attributes = {};
    attributes.Size = sizeof(attributes);
    if(!HidD_GetAttributes(handle.value, &attributes) || attributes.VendorID != 0x0B05 || attributes.ProductID != 0x1AFA)
    {
        error = "Delta II identity mismatch"; return false;
    }
    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if(!HidD_GetPreparsedData(handle.value, &preparsed)) { error = "HID descriptor unavailable"; return false; }
    HIDP_CAPS caps = {};
    const auto result = HidP_GetCaps(preparsed, &caps);
    HidD_FreePreparsedData(preparsed);
    if(result != HIDP_STATUS_SUCCESS || !BatteryProtocol::AsusCollection(caps.UsagePage, caps.Usage,
       caps.InputReportByteLength, caps.OutputReportByteLength))
    {
        error = "Unexpected Delta II vendor report descriptor; no commands sent"; return false;
    }
    return true;
}

int Transfer(Handle& device, bool write, unsigned char* buffer, DWORD size, DWORD timeout)
{
    return DeltaTransfer().Transfer(device.value, write, buffer, size, timeout);
}
bool Query(Handle& device, unsigned char key, std::array<unsigned char,64>& reply, std::string& error)
{
    const auto deadline = Clock::now() + std::chrono::milliseconds(250);
    std::array<unsigned char,64> request = {};
    request[0] = 0xCC; request[1] = 0x12; request[2] = key;
    if(Transfer(device, true, request.data(), request.size(), 100) != 64)
    {
        error = "Battery request did not complete"; return false;
    }
    for(unsigned int i = 0; i < 4 && Clock::now() < deadline; ++i)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if(remaining <= 0) { break; }
        const int count = Transfer(device, false, reply.data(), reply.size(), static_cast<DWORD>(remaining));
        if(count < 0) { error = "Battery response transport failed"; return false; }
        if(count == 0) { break; }
        if(BatteryProtocol::AsusReply(reply.data(), static_cast<size_t>(count), key))
        {
            if(key == 7 && count < 7) { continue; }
            return true;
        }
        if(count >= 3 && reply[0] == 0xCC && reply[1] == 0xFF && reply[2] == 0xAA)
        {
            error = "Headset refused the request"; return false;
        }
        // Other events are not treated as a matching battery response.
    }
    error = "No valid matching battery reply before deadline";
    return false;
}
}
#endif

nlohmann::json AsusDeltaIIBattery::ReadAwakeSnapshot(bool owner_confirmed_awake)
{
    nlohmann::json result = {{"device", "ASUS ROG Delta II"}, {"percentage", nullptr}, {"charging", nullptr},
        {"automatic_polling_enabled", false}, {"no_wake_validated", false}};
    if(!owner_confirmed_awake)
    {
        result["error"] = "Owner confirmation that the headset is awake is required; no interface opened";
        return result;
    }
#ifdef _WIN32
    std::lock_guard<std::mutex> owner(delta_mutex);
    Handle handle;
    std::string error;
    if(!OpenExactDelta(handle, error)) { result["error"] = error; return result; }
    std::array<unsigned char,64> reply = {};
    if(!Query(handle, 7, reply, error)) { result["error"] = error; return result; }
    const auto percentage = BatteryProtocol::AsusLevel(reply.data(), reply.size());
    if(!percentage) { result["error"] = "Invalid battery percentage reply"; return result; }
    result["percentage"] = *percentage;
    result["level_report_prefix"] = std::vector<unsigned char>(reply.begin(), reply.begin() + 12);
    if(Query(handle, 8, reply, error))
    {
        const auto charging = BatteryProtocol::AsusCharging(reply.data(), reply.size());
        if(charging) { result["charging"] = *charging == 1; }
        result["charging_report_prefix"] = std::vector<unsigned char>(reply.begin(), reply.begin() + 12);
    }
    else { result["charging_error"] = error; }
    result["firmware_usb_version"] = "Not queried: no unrelated commands";
#else
    result["error"] = "Owner-hardware validation is currently Windows-only";
#endif
    return result;
}

nlohmann::json AsusDeltaIIBattery::ReadAuthorizedSleepTest(bool owner_authorized_one_query)
{
    nlohmann::json result = {{"device", "ASUS ROG Delta II"}, {"percentage", nullptr}, {"charging", nullptr},
        {"sleep_safety_experiment", true}, {"level_query_attempted", false},
        {"automatic_polling_enabled", false}, {"no_wake_validated", false}};
    if(!owner_authorized_one_query)
    {
        result["error"] = "Explicit authorization for one asleep battery query is required; no interface opened";
        return result;
    }
#ifdef _WIN32
    std::lock_guard<std::mutex> owner(delta_mutex);
    Handle handle;
    std::string error;
    if(!OpenExactDelta(handle, error)) { result["error"] = error; return result; }
    std::array<unsigned char,64> reply = {};
    result["level_query_attempted"] = true;
    // Exactly one documented battery request. Do not query charging or retry.
    if(!Query(handle, 7, reply, error)) { result["error"] = error; return result; }
    const auto percentage = BatteryProtocol::AsusLevel(reply.data(), reply.size());
    if(!percentage) { result["error"] = "Invalid battery percentage reply"; return result; }
    result["percentage"] = *percentage;
    result["level_report_prefix"] = std::vector<unsigned char>(reply.begin(), reply.begin() + 12);
    result["observation_required"] = "Owner must report whether the naturally asleep headset woke; a reply does not prove no-wake";
#else
    result["error"] = "Owner-hardware validation is currently Windows-only";
#endif
    return result;
}
nlohmann::json AsusDeltaIIBattery::Observe()
{
    nlohmann::json result = {{"device", "ASUS ROG Delta II"}, {"reports", nlohmann::json::array()},
        {"commands_sent", 0}, {"automatic_polling_enabled", false}, {"no_wake_validated", false}};
#ifdef _WIN32
    std::lock_guard<std::mutex> owner(delta_mutex);
    Handle handle;
    std::string error;
    if(!OpenExactDelta(handle, error)) { result["error"] = error; return result; }
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    for(unsigned int i = 0; i < 16 && Clock::now() < deadline; ++i)
    {
        std::array<unsigned char,64> report = {};
        const int count = Transfer(handle, false, report.data(), report.size(), 100);
        if(count < 0) { result["error"] = "Passive input unavailable"; break; }
        if(count > 0)
        {
            result["reports"].push_back(std::vector<unsigned char>(report.begin(), report.begin() + std::min(count,16)));
        }
    }
#else
    result["error"] = "Owner-hardware validation is currently Windows-only";
#endif
    return result;
}

nlohmann::json AsusDeltaIIBattery::InspectAvailability()
{
    nlohmann::json result = {{"device", "ASUS ROG Delta II"}, {"commands_sent", 0},
        {"automatic_polling_enabled", false}};
#ifdef _WIN32
    std::lock_guard<std::mutex> owner(delta_mutex);
    Handle handle;
    std::string error;
    result["exclusive_interface_available"] = OpenExactDelta(handle, error);
    if(!error.empty()) { result["error"] = error; }
#else
    result["error"] = "Interface inspection is currently Windows-only";
#endif
    return result;
}

nlohmann::json AsusDeltaIIBattery::ReadValidatedReceiverSnapshot(bool sleep_safe_level_query_validated)
{
    nlohmann::json result = {{"device", "ASUS ROG Delta II"}, {"percentage", nullptr},
        {"charging", nullptr}, {"availability", "unavailable"}};
    if(!sleep_safe_level_query_validated)
    {
        result["error"] = "Automatic queries disabled: sleep-safe level acquisition not validated; no interface opened";
        return result;
    }
#ifdef _WIN32
    std::lock_guard<std::mutex> owner(delta_mutex);
    Handle handle;
    std::string error;
    if(!OpenExactDelta(handle, error))
    {
        if(error == "Exact Delta II vendor collection not present") { result["availability"] = "disconnected"; }
        result["error"] = error;
        return result;
    }
    std::array<unsigned char,64> reply = {};
    // Validation applies only to this level request, never to charging/settings.
    if(!Query(handle, 7, reply, error)) { result["error"] = error; return result; }
    const auto percentage = BatteryProtocol::AsusLevel(reply.data(), reply.size());
    if(!percentage) { result["error"] = "Invalid battery percentage reply"; return result; }
    result["percentage"] = *percentage;
    result["availability"] = "receiver";
#else
    result["error"] = "Delta II acquisition is currently Windows-only";
#endif
    return result;
}
