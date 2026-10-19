/*---------------------------------------------------------*\
| RGBController_AsusAuraNeo.h                               |
|                                                           |
| ASUS Aura NEO onboard logo controller                     |
|                                                           |
| valer23                                     12 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#pragma once

#include "RGBController.h"
#include <hidapi.h>

#define ASUS_AURA_NEO_REPORT_SIZE 64
#define ASUS_AURA_NEO_MODE_STATIC 0xFFFF

/*---------------------------------------------------------*\
| Only the onboard logo has a confirmed colour response.    |
| Addressable-header reports are not sent.                  |
\*---------------------------------------------------------*/
class RGBController_AsusAuraNeo : public RGBController
{
public:
    RGBController_AsusAuraNeo(hid_device* device, const std::string& path);
    ~RGBController_AsusAuraNeo();

    RGBController_AsusAuraNeo(const RGBController_AsusAuraNeo&) = delete;
    RGBController_AsusAuraNeo& operator=(const RGBController_AsusAuraNeo&) = delete;

    void DeviceUpdateLEDs() override;
    void DeviceUpdateZoneLEDs(int zone) override;
    void DeviceUpdateSingleLED(int led) override;
    void DeviceUpdateMode() override;

private:
    bool                    Send(const unsigned char* report);
    bool                    SendOnboardReport(unsigned char output, bool initialize, RGBColor color);
    bool                    Initialize();
    hid_device*             device;
    bool                    write_failed = false;
    bool                    initialized = false;
};
