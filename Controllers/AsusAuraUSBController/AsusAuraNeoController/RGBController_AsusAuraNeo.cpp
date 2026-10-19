/*---------------------------------------------------------*\
| RGBController_AsusAuraNeo.cpp                             |
|                                                           |
| ASUS Aura NEO onboard logo controller                     |
|                                                           |
| valer23                                     12 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#include "RGBController_AsusAuraNeo.h"
#include "LogManager.h"
#include <chrono>
#include <thread>

#if defined(HID_API_VERSION) && HID_API_VERSION >= 0x00000F00

/**------------------------------------------------------------------*\
    @name ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)
    @category Motherboard
    @type USB
    @save :o:
    @direct :white_check_mark:
    @effects :o:
    @detectors DetectAsusAuraNeo
    @comment Requires HIDAPI 0.15 or newer. Verified on Windows hardware.
             Static onboard-logo colour streaming is supported.
             ARGB headers and hardware effects are unsupported.
             Avoid simultaneous ASUS LightingService lighting control.
\*-------------------------------------------------------------------*/

RGBController_AsusAuraNeo::RGBController_AsusAuraNeo(hid_device* handle, const std::string& path)
{
    device      = handle;
    name        = "ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)";
    vendor      = "ASUS";
    type        = DEVICE_TYPE_MOTHERBOARD;
    description = "Onboard logo only; captured 0B05:1CD2 protocol";
    location    = "HID: " + path;
    mode static_mode;
    static_mode.name       = "Static";
    static_mode.value      = ASUS_AURA_NEO_MODE_STATIC;
    static_mode.flags      = MODE_FLAG_HAS_PER_LED_COLOR;
    static_mode.color_mode = MODE_COLORS_PER_LED;
    modes.push_back(static_mode);
    zone logo;
    logo.name       = "ROG Logo";
    logo.type       = ZONE_TYPE_SINGLE;
    logo.leds_min   = 1;
    logo.leds_max   = 1;
    logo.leds_count = 1;
    zones.push_back(logo);
    led logo_led;
    logo_led.name = "ROG Logo";
    leds.push_back(logo_led);
    SetupColors();
}

RGBController_AsusAuraNeo::~RGBController_AsusAuraNeo()
{
    Shutdown();
    hid_close(device);
}

bool RGBController_AsusAuraNeo::SendOnboardReport(unsigned char output, bool initialize, RGBColor color)
{
    unsigned char report[ASUS_AURA_NEO_REPORT_SIZE] = {};
    report[0] = 0x41;
    report[1] = 0x05;
    report[2] = 0x01;
    report[6] = output;
    if(initialize)
    {
        report[8] = 0x01;
    }
    else
    {
        report[16] = 0x01;
        report[18] = RGBGetGValue(color);
        report[19] = RGBGetRValue(color);
        report[20] = RGBGetBValue(color);
    }
    return Send(report);
}

bool RGBController_AsusAuraNeo::Send(const unsigned char* report)
{
    /*---------------------------------------------------------*\
    | The capture uses control SET_REPORT, not interrupt output.|
    \*---------------------------------------------------------*/
    if(hid_send_output_report(device, report, ASUS_AURA_NEO_REPORT_SIZE) != ASUS_AURA_NEO_REPORT_SIZE)
    {
        if(!write_failed)
        {
            LOG_ERROR("[ASUS Aura NEO] Output report 0x%02X failed", report[1]);
        }
        write_failed = true;
        initialized = false;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    return true;
}

bool RGBController_AsusAuraNeo::Initialize()
{
    /*---------------------------------------------------------*\
    | Captured onboard setup only. Header setup is excluded.   |
    \*---------------------------------------------------------*/
    if(!SendOnboardReport(0, true, 0) || !SendOnboardReport(1, true, 0))
    {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2200));
    unsigned char activate[ASUS_AURA_NEO_REPORT_SIZE] = {};
    activate[0] = 0x41;
    activate[1] = 0x21;
    /*---------------------------------------------------------*\
    | Captured activation packet; its wider meaning is unknown.|
    \*---------------------------------------------------------*/
    activate[3] = 0x01;
    if(!Send(activate))
    {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    initialized = true;
    return true;
}

void RGBController_AsusAuraNeo::DeviceUpdateLEDs()
{
    if(colors.empty() || (size_t)active_mode >= modes.size() || modes[active_mode].value != ASUS_AURA_NEO_MODE_STATIC || (!initialized && !Initialize()))
    {
        return;
    }
    /*---------------------------------------------------------*\
    | Both captured outputs share the logo colour. Their       |
    | separate physical mapping has not been established.      |
    \*---------------------------------------------------------*/
    if(!SendOnboardReport(0, false, colors[0]))
    {
        return;
    }
    if(SendOnboardReport(1, false, colors[0]))
    {
        write_failed = false;
    }
}

void RGBController_AsusAuraNeo::DeviceUpdateZoneLEDs(int zone)
{
    if(zone == 0)
    {
        DeviceUpdateLEDs();
    }
}

void RGBController_AsusAuraNeo::DeviceUpdateSingleLED(int led)
{
    if(led == 0)
    {
        DeviceUpdateLEDs();
    }
}

void RGBController_AsusAuraNeo::DeviceUpdateMode()
{
    DeviceUpdateLEDs();
}

#endif
