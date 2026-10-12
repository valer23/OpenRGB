/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Brimstone.h"
#include "BrimstonePattern.h"

REGISTER_EFFECT(Brimstone);

Brimstone::Brimstone() : RGBEffect()
{
    EffectDetails.EffectName = UI_Name();
    EffectDetails.EffectClassName = ClassName();
    EffectDetails.EffectDescription = "Orange background, ember rain and opposing waves adapted from Lewis Gerschwitz's iCUE Brimstone profile.";
    EffectDetails.MinSpeed = 10;
    EffectDetails.MaxSpeed = 200;
    EffectDetails.SupportsRandom = false;
    SetSpeed(100);
    SetFPS(10);
}

void Brimstone::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    for(ControllerZone* zone : controller_zones)
    {
        const unsigned int count = zone->leds_count();
        for(unsigned int led = 0; led < count; led++)
        {
            double position = count > 1 ? (double)led / (count - 1) : 0.0;
            if(zone->reverse)
            {
                position = 1.0 - position;
            }
            // The logo/GPU participate as one zone; individual LEDs use
            // repeatable ember phases. Matrix positions follow keyboard columns.
            if(zone->type() == ZONE_TYPE_MATRIX || zone->type() == ZONE_TYPE_MATRIX_LOOP_X || zone->type() == ZONE_TYPE_MATRIX_LOOP_Y)
            {
                const unsigned int width = zone->matrix_map_width();
                const unsigned int height = zone->matrix_map_height();
                for(unsigned int cell = 0; width && cell < width * height; cell++)
                {
                    if(zone->map()[cell] == led)
                    {
                        position = width > 1 ? (double)(cell % width) / (width - 1) : 0.0;
                        if(zone->reverse) position = 1.0 - position;
                        break;
                    }
                }
            }
            const BrimstoneColor color = BrimstonePattern::Sample(seconds, position, led);
            zone->SetLED((int)led, ToRGBColor((unsigned int)std::lround(color.red),
                (unsigned int)std::lround(color.green), (unsigned int)std::lround(color.blue)), Brightness, Temperature, Tint);
        }
    }
    if(FPS)
    {
        seconds += (double)Speed / (100.0 * FPS);
    }
}
