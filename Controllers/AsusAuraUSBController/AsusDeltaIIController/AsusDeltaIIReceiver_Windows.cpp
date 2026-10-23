/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "AsusDeltaIIReceiver.h"
#include "BatteryStatus/BatteryProtocol.h"
#include "BatteryStatus/WindowsHidTransfer.h"
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

namespace
{
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

class DeltaIo final : public AsusDeltaIIReceiver::Io
{
public:
    bool Ready() override { return DeltaTransfer().Ready(); }
    bool Open(std::string& error) override { return OpenExactDelta(handle, error); }
    void Close() override
    {
        if(handle.value != INVALID_HANDLE_VALUE && handle.value != nullptr)
        {
            CloseHandle(handle.value);
            handle.value = INVALID_HANDLE_VALUE;
        }
    }
    int Transfer(bool write, unsigned char* data, unsigned int size, unsigned int timeout) override
    {
        return DeltaTransfer().Transfer(handle.value, write, data, size, timeout);
    }
private:
    Handle handle;
};
}

AsusDeltaIIReceiver& GetAsusDeltaIIReceiver()
{
    // Process lifetime, matching WindowsHidTransfer: pending cancelled driver
    // operations may still own their buffers and device during core teardown.
    static auto* channel = new DeltaIo;
    static auto* receiver = new AsusDeltaIIReceiver(*channel);
    return *receiver;
}
#endif
