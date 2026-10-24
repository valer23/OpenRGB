/*---------------------------------------------------------*\
| RGBController_AsusAuraNeo_Windows.h                       |
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
#include <array>

/* Implemented beside the Windows detector; also used by offline tests. */
bool AsusAuraNeoHasCompetingLighting();

/* Only the onboard logo of 0B05:1CD2 has a confirmed colour response.
 * This controller never sends addressable-header configuration or colours. */
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
    void DeviceSaveMode() override;

private:
    using Report = std::array<unsigned char, 64>;
    hid_device* device;
    bool write_failed = false;
    bool initialized = false;
    bool Send(const Report& report);
    bool Initialize();
    bool Query(const Report& request, Report& response);
    static Report OnboardReport(unsigned char output, bool initialize, RGBColor color);
};
