/*---------------------------------------------------------*\
| AsusAuraNeoControllerDetect_Windows.cpp                   |
|                                                           |
| ASUS Aura NEO onboard logo detector                       |
|                                                           |
| valer23                                     12 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#include "DetectionManager.h"
#include "RGBController_AsusAuraNeo_Windows.h"
#include <memory>
#include <windows.h>
#include <tlhelp32.h>

bool AsusAuraNeoHasCompetingLighting()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(snapshot == INVALID_HANDLE_VALUE)
    {
        return true;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    bool competing = false;
    if(!Process32FirstW(snapshot, &entry))
    {
        CloseHandle(snapshot);
        return true;
    }
    do
    {
        if(lstrcmpiW(entry.szExeFile, L"LightingService.exe") == 0 ||
           lstrcmpiW(entry.szExeFile, L"AsusMBManager.exe") == 0)
        {
            competing = true;
            break;
        }
    } while(Process32NextW(snapshot, &entry));
    if(!competing && GetLastError() != ERROR_NO_MORE_FILES)
    {
        competing = true;
    }
    CloseHandle(snapshot);
    return competing;
}

DetectedControllers DetectAsusAuraNeo(hid_device_info* info, const std::string&)
{
    DetectedControllers found;
    // Keep a second guard even though the detector registration filters these.
    if(info->vendor_id != 0x0B05 || info->product_id != 0x1CD2 ||
       info->interface_number != 1 || info->usage_page != 0xFF0A || info->usage != 1)
    {
        return found;
    }
    std::unique_ptr<hid_device, decltype(&hid_close)> device(hid_open_path(info->path), hid_close);
    if(device)
    {
        std::unique_ptr<RGBController_AsusAuraNeo> controller = std::make_unique<RGBController_AsusAuraNeo>(device.get(), info->path);
        device.release();
        found.push_back(controller.get());
        controller.release();
    }
    return found;
}

REGISTER_HID_DETECTOR_IPU("ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)", DetectAsusAuraNeo,
                          0x0B05, 0x1CD2, 1, 0xFF0A, 1);
