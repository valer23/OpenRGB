/*---------------------------------------------------------*\
| RGBController_GalaxGPUv1.cpp                              |
|                                                           |
|   RGBController for Galax/KFA2 GPU                        |
|                                                           |
|   Niels Westphal (crashniels)                 12 Jul 2020 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "RGBController_GalaxGPUv1.h"
#include "LogManager.h"

int RGBController_GalaxGPUv1::GetDeviceMode()
{
    if(controller->IsEXGamer())
    {
        int sync = controller->GetEXSync();
        if(sync < 0 || sync > 1)
        {
            LOG_WARNING("[KFA2 EX] Sync state unavailable (%d); presenting Static until a mode is selected", sync);
        }
        return sync == 1 ? 1 : 0;
    }
    int modereg1 = controller->GalaxGPURegisterRead(GALAX_V1_MODE_REGISTER_1);
    int modereg2 = controller->GalaxGPURegisterRead(GALAX_V1_MODE_REGISTER_2);

    if(modereg1 == GALAX_V1_MODE_STATIC_VALUE_1 && modereg2 == GALAX_V1_MODE_STATIC_VALUE_2)
    {
        active_mode = 1;
        modes[active_mode].color_mode = MODE_COLORS_PER_LED;
    }

    if(modereg1 == GALAX_V1_MODE_BREATHING_VALUE_1 && modereg2 == GALAX_V1_MODE_BREATHING_VALUE_2)
    {
        active_mode = 2;
        modes[active_mode].color_mode = MODE_COLORS_PER_LED;
    }

    if(modereg1 == GALAX_V1_MODE_RAINBOW_VALUE_1 && modereg2 == GALAX_V1_MODE_RAINBOW_VALUE_2)
    {
        active_mode = 3;
        modes[active_mode].color_mode = MODE_COLORS_NONE;
    }

    if(modereg1 == GALAX_V1_MODE_CYCLE_BREATHING_VALUE_1 && modereg2 == GALAX_V1_MODE_CYCLE_BREATHING_VALUE_2)
    {
        active_mode = 4;
        modes[active_mode].color_mode = MODE_COLORS_NONE;
    }

    return(active_mode);
}

/**------------------------------------------------------------------*\
    @name Galax GPU v1
    @category GPU
    @type I2C
    @save :x:
    @direct :white_check_mark:
    @effects :white_check_mark:
    @detectors DetectGalaxGPUControllers
    @comment KFA2 RTX 3080 Ti EX Gamer exposes Static and Motherboard Sync only.
        Hardware effects and external motherboard-sync colour following are unverified.
\*-------------------------------------------------------------------*/

RGBController_GalaxGPUv1::RGBController_GalaxGPUv1(GalaxGPUv1Controller* controller_ptr)
{
    controller                  = controller_ptr;

    name                        = controller->GetDeviceName();
    vendor                      = controller->IsEXGamer() ? "KFA2" : "GALAX";
    type                        = DEVICE_TYPE_GPU;
    description                 = controller->IsEXGamer() ? "KFA2 EX Gamer I2C lighting controller" : "GALAX / KFA2 RTX GPU";
    location                    = controller->GetDeviceLocation();

    mode Direct;
    Direct.name                 = controller->IsEXGamer() ? "Static" : "Direct";
    Direct.value                = 1;
    Direct.flags                = MODE_FLAG_HAS_PER_LED_COLOR;
    Direct.color_mode           = MODE_COLORS_PER_LED;
    modes.push_back(Direct);

    if(controller->IsEXGamer())
    {
        mode sync;
        sync.name       = "Motherboard Sync";
        sync.value      = GALAX_V1_EX_SYNC_MODE;
        sync.flags      = 0;
        sync.color_mode = MODE_COLORS_NONE;
        modes.push_back(sync);
    }
    else
    {
        mode Breathing;
        Breathing.name              = "Breathing";
        Breathing.value             = 2;
        Breathing.flags             = MODE_FLAG_HAS_PER_LED_COLOR;
        Breathing.color_mode        = MODE_COLORS_PER_LED;
        modes.push_back(Breathing);

        mode Rainbow;
        Rainbow.name                = "Rainbow";
        Rainbow.value               = 3;
        Rainbow.flags               = 0;
        Rainbow.color_mode          = MODE_COLORS_NONE;
        modes.push_back(Rainbow);

        mode Cycle_Breathing;
        Cycle_Breathing.name        = "Cycle Breathing";
        Cycle_Breathing.value       = 4;
        Cycle_Breathing.flags       = 0;
        Cycle_Breathing.color_mode  = MODE_COLORS_NONE;
        modes.push_back(Cycle_Breathing);
    }

    try
    {
        SetupZones();
        active_mode = GetDeviceMode();
    }
    catch(...)
    {
        Shutdown();
        throw;
    }
}

RGBController_GalaxGPUv1::~RGBController_GalaxGPUv1()
{
    Shutdown();

    delete controller;
}

void RGBController_GalaxGPUv1::SetupZones()
{
    /*---------------------------------------------------------*\
    | Set up zone                                               |
    \*---------------------------------------------------------*/
    zone galax_gpu_zone;
    galax_gpu_zone.name         = "GPU";
    galax_gpu_zone.type         = ZONE_TYPE_SINGLE;
    galax_gpu_zone.leds_min     = 1;
    galax_gpu_zone.leds_max     = 1;
    galax_gpu_zone.leds_count   = 1;
    zones.push_back(galax_gpu_zone);

    /*---------------------------------------------------------*\
    | Set up LED                                                |
    \*---------------------------------------------------------*/
    led galax_gpu_led;
    galax_gpu_led.name          = "GPU";
    leds.push_back(galax_gpu_led);

    SetupColors();

    /*---------------------------------------------------------*\
    | Initialize color                                          |
    \*---------------------------------------------------------*/
    unsigned char red, grn, blu;
    controller->GetLEDColors(red, grn, blu);

    colors[0] = ToRGBColor(red, grn, blu);
}

void RGBController_GalaxGPUv1::DeviceUpdateLEDs()
{
    std::lock_guard<std::mutex> lock(io_mutex);
    if(controller->IsEXGamer())
    {
        if(active_mode != 0 || colors.empty())
        {
            return;
        }
        try
        {
            controller->SetLEDColorsDirect(RGBGetRValue(colors[0]), RGBGetGValue(colors[0]), RGBGetBValue(colors[0]));
        }
        catch(const std::exception& error)
        {
            LOG_ERROR("[KFA2 EX] %s", error.what());
        }
        return;
    }
    for(std::size_t led = 0; led < colors.size(); led++)
    {
        unsigned char red = RGBGetRValue(colors[led]);
        unsigned char grn = RGBGetGValue(colors[led]);
        unsigned char blu = RGBGetBValue(colors[led]);

        if(GetActiveMode() == 1)
        {
            controller->SetLEDColorsDirect(red, grn, blu);
        }
        else
        {
            controller->SetLEDColorsEffect(red, grn, blu);
        }
    }
}

void RGBController_GalaxGPUv1::DeviceUpdateZoneLEDs(int zone)
{
    if(controller->IsEXGamer() && zone != 0)
    {
        return;
    }
    DeviceUpdateLEDs();
}

void RGBController_GalaxGPUv1::DeviceUpdateSingleLED(int led)
{
    if(controller->IsEXGamer() && led != 0)
    {
        return;
    }
    DeviceUpdateLEDs();
}

void RGBController_GalaxGPUv1::DeviceUpdateMode()
{
    std::lock_guard<std::mutex> lock(io_mutex);
    int new_mode = modes[active_mode].value;
    if(controller->IsEXGamer())
    {
        try
        {
            controller->SetMode(new_mode);
            if(active_mode == 0 && !colors.empty())
            {
                controller->SetLEDColorsDirect(RGBGetRValue(colors[0]), RGBGetGValue(colors[0]), RGBGetBValue(colors[0]));
            }
        }
        catch(const std::exception& error)
        {
            LOG_ERROR("[KFA2 EX] %s", error.what());
        }
        return;
    }
    controller->SetMode(new_mode);
}
