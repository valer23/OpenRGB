/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RGBControllerInterface.h"

// Only these captured, volatile colour controllers are allowed to use Static
// as the streaming mode. Do not enable arbitrary firmware-backed Static modes.
inline int CompatibleDirectMode(RGBControllerInterface* controller)
{
    for(unsigned int i = 0; i < controller->GetModeCount(); i++)
    {
        if(controller->GetModeName(i) == "Direct")
        {
            return (int)i;
        }
    }
    const std::string name = controller->GetName();
    // The existing SL-Infinity controller calls its per-LED streaming mode Custom.
    if(name == "Lian Li Uni Hub - SL Infinity")
    {
        for(unsigned int i = 0; i < controller->GetModeCount(); i++)
        {
            if(controller->GetModeName(i) == "Custom" &&
               (controller->GetModeFlags(i) & MODE_FLAG_HAS_PER_LED_COLOR))
            {
                return (int)i;
            }
        }
        return -1;
    }
    if(name != "KFA2 GeForce RTX 3080 Ti EX Gamer" &&
       name != "ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)" &&
       name != "Lian Li SL Infinity (local static test)")
    {
        return -1;
    }
    for(unsigned int i = 0; i < controller->GetModeCount(); i++)
    {
        if(controller->GetModeName(i) == "Static" &&
           (controller->GetModeFlags(i) & MODE_FLAG_HAS_PER_LED_COLOR))
        {
            return (int)i;
        }
    }
    return -1;
}
