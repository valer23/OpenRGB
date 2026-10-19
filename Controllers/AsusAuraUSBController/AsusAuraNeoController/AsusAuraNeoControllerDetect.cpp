/*---------------------------------------------------------*\
| AsusAuraNeoControllerDetect.cpp                           |
|                                                           |
| ASUS Aura NEO onboard logo detector                       |
|                                                           |
| valer23                                     12 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#include "DetectionManager.h"
#include "RGBController_AsusAuraNeo.h"

#define ASUS_AURA_NEO_VID 0x0B05
#define ASUS_AURA_NEO_PID 0x1CD2

/*---------------------------------------------------------*\
| HIDAPI 0.15 provides the required control-output transport.|
\*---------------------------------------------------------*/
#if defined(HID_API_VERSION) && HID_API_VERSION >= 0x00000F00
DetectedControllers DetectAsusAuraNeo(hid_device_info* info, const std::string&)
{
    DetectedControllers             detected_controllers;
    hid_device*                     dev = hid_open_path(info->path);

    if(dev)
    {
        RGBController_AsusAuraNeo*   controller = new RGBController_AsusAuraNeo(dev, info->path);
        detected_controllers.push_back(controller);
    }

    return(detected_controllers);
}

REGISTER_HID_DETECTOR_IPU("ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)", DetectAsusAuraNeo, ASUS_AURA_NEO_VID, ASUS_AURA_NEO_PID, 1, 0xFF0A, 1);
#endif
