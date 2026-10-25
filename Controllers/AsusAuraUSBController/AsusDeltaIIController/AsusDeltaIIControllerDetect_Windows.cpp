/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "DetectionManager.h"
#include "StringUtils.h"
#include "AsusDeltaIIDetection.h"
#include "RGBController_AsusDeltaII_Windows.h"

DetectedControllers DetectAsusDeltaII(hid_device_info* info,const std::string&)
{
    if(!IsAsusDeltaIICollection(info->vendor_id,info->product_id,info->interface_number,info->usage_page,info->usage,info->path))return {};
    // Cached HID metadata only. Construction neither opens the receiver nor
    // reads a report, initializes lighting, observes audio or changes services.
    const std::string serial=info->serial_number ? StringUtils::wstring_to_string(info->serial_number) : "";
    return {new RGBController_AsusDeltaII(info->path,serial)};
}
REGISTER_HID_DETECTOR_IPU("ASUS ROG Delta II",DetectAsusDeltaII,0x0B05,0x1AFA,3,0xFF00,1);