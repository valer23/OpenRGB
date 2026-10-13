/*---------------------------------------------------------*\
| RGBController_AsusAuraNeo_Windows.cpp                     |
|                                                           |
| ASUS Aura NEO onboard logo controller                     |
|                                                           |
| valer23                                     12 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#include "RGBController_AsusAuraNeo_Windows.h"
#include "LogManager.h"
#include <chrono>
#include <thread>

/**------------------------------------------------------------------*\
    @name ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)
    @category Motherboard
    @type USB
    @save :o:
    @direct :white_check_mark:
    @effects :o:
    @detectors DetectAsusAuraNeo
    @comment Windows only. Static onboard-logo colour streaming is supported.
             ARGB headers and hardware effects are unsupported.
             Avoid simultaneous ASUS LightingService lighting control.
\*-------------------------------------------------------------------*/

RGBController_AsusAuraNeo::RGBController_AsusAuraNeo(hid_device* handle, const std::string& path)
    : device(handle)
{
    name        = "ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)";
    vendor      = "ASUS";
    type        = DEVICE_TYPE_MOTHERBOARD;
    description = "Onboard logo only; captured 0B05:1CD2 protocol";
    location    = "HID: " + path;
    mode static_mode;
    static_mode.name       = "Static";
    static_mode.value      = 0;
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

RGBController_AsusAuraNeo::Report RGBController_AsusAuraNeo::OnboardReport(
    unsigned char output, bool initialize, RGBColor color)
{
    Report report = {};
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
    return report;
}

bool RGBController_AsusAuraNeo::Send(const Report& report)
{
    // The capture uses control SET_REPORT. hid_write uses a different endpoint.
    if(hid_send_output_report(device, report.data(), report.size()) != static_cast<int>(report.size()))
    {
        LOG_ERROR("[ASUS Aura NEO] Output report 0x%02X failed", report[1]);
        initialized = false;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    return true;
}

bool RGBController_AsusAuraNeo::Initialize()
{
    // Exact captured onboard setup packets, frames 369 and 371. Header setup
    // packets are deliberately excluded; this subset needs physical validation.
    if(!Send(OnboardReport(0, true, 0)) || !Send(OnboardReport(1, true, 0)))
    {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2200));
    Report activate = {};
    activate[0] = 0x41;
    activate[1] = 0x21;
    activate[3] = 0x01; // Exact captured frame 407; activation semantics inferred.
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
    std::lock_guard<std::mutex> lock(io_mutex);
    if(colors.empty() || active_mode != 0 || (!initialized && !Initialize()))
    {
        return;
    }
    // Both captured onboard outputs share the chosen logo colour. Their separate
    // physical mapping is unknown, so do not expose unverified independent zones.
    if(!Send(OnboardReport(0, false, colors[0])))
    {
        return;
    }
    Send(OnboardReport(1, false, colors[0]));
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
