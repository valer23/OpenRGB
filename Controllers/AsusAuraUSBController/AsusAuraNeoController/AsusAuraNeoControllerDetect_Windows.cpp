/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "DetectionManager.h"
#include "RGBController_AsusAuraNeo_Windows.h"
#include <memory>

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
        auto controller = std::make_unique<RGBController_AsusAuraNeo>(device.get(), info->path);
        device.release();
        found.push_back(controller.get());
        controller.release();
    }
    return found;
}

REGISTER_HID_DETECTOR_IPU("ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)", DetectAsusAuraNeo,
                          0x0B05, 0x1CD2, 1, 0xFF0A, 1);
