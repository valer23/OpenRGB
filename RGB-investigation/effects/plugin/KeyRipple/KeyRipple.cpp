/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "KeyRipple.h"
#ifdef _WIN32
#include "LightMountKeys.h"
#include "RipplePattern.h"
#include "OpenRGBEffectsPlugin.h"
#include <map>

REGISTER_EFFECT(KeyRipple);

KeyRipple::KeyRipple() : RGBEffect()
{
    EffectDetails.EffectName = UI_Name();
    EffectDetails.EffectClassName = ClassName();
    EffectDetails.EffectDescription = "White/purple key-triggered ripples for the 135-lamp be quiet! Light Mount. First colour is the base; second is the ripple. Works while minimized.";
    EffectDetails.SupportsRandom = false;
    EffectDetails.UserColors = 2;
    EffectDetails.MinSpeed = 10;
    EffectDetails.MaxSpeed = 200;
    EffectDetails.HasCustomSettings = true;
    SetUserColors({ToRGBColor(255,255,255), ToRGBColor(160,0,255)});
    SetSpeed(100); SetFPS(30);
}

void KeyRipple::EffectState(bool enabled)
{
    if(enabled)
    {
        if(!input.Start()) LOG_WARNING("[KeyRipple] Keyboard input registration failed or another ripple is already listening.");
    }
    else input.Stop();
    RGBEffect::EffectState(enabled);
}

void KeyRipple::LoadCustomSettings(json settings)
{
    try
    {
        const double new_hold = settings.value("highlight_ms",100.0) / 1000;
        const double new_duration = settings.value("duration_ms",800.0) / 1000;
        const double new_width = settings.value("width",1.8);
        if(!std::isfinite(new_hold) || !std::isfinite(new_duration) || !std::isfinite(new_width) ||
           new_hold < 0 || new_hold > 1 || new_duration < 0.1 || new_duration > 5 || new_width < 0.5 || new_width > 5) return;
        hold = new_hold; duration = new_duration; width = new_width;
    }
    catch(const std::exception&) {}
}

json KeyRipple::SaveCustomSettings()
{
    return {{"highlight_ms",hold*1000},{"duration_ms",duration*1000},{"width",width}};
}

void KeyRipple::StepEffect(std::vector<ControllerZone*> zones)
{
    const auto now = RawKeyboardInput::Clock::now();
    const auto presses = input.Drain();
    for(const auto& press : presses)
    {
        if(LightMountLamp(press.scan) < 0) continue;
        if(waves.size() == 64) waves.erase(waves.begin());
        waves.push_back(press);
    }
    const double timing = std::max(0.1, Speed / 100.0);
    waves.erase(std::remove_if(waves.begin(),waves.end(),[&](const auto& wave) {
        return std::chrono::duration<double>(now-wave.time).count()*timing >= std::max(hold,duration);
    }),waves.end());
    const RGBColor base = UserColors.size() > 0 ? UserColors[0] : ToRGBColor(255,255,255);
    const RGBColor purple = UserColors.size() > 1 ? UserColors[1] : ToRGBColor(160,0,255);
    for(ControllerZone* zone : zones)
    {
        if(zone->controller->GetName() != "be quiet! Light Mount" || zone->is_segment || zone->leds_count()!=135 ||
           zone->matrix_map_width()!=84 || zone->matrix_map_height()!=7) continue;
        std::map<unsigned int,std::pair<double,double>> positions;
        for(unsigned int row=0;row<7;row++) for(unsigned int col=0;col<84;col++)
        {
            const unsigned int led = zone->map()[row*84+col];
            if(led<135) positions[led]={col/5.0,static_cast<double>(row)};
        }
        struct ActiveWave { unsigned int origin; double x,y,radius,age; };
        std::vector<ActiveWave> active;
        for(const auto& wave : waves)
        {
            const unsigned int origin=static_cast<unsigned int>(LightMountLamp(wave.scan));
            const auto centre=positions.find(origin);
            if(centre==positions.end()) continue;
            double radius=0;
            for(const auto& entry : positions) radius=std::max(radius,std::hypot(entry.second.first-centre->second.first,entry.second.second-centre->second.second));
            active.push_back({origin,centre->second.first,centre->second.second,radius+width,std::chrono::duration<double>(now-wave.time).count()*timing});
        }
        for(unsigned int led=0;led<135;led++)
        {
            double intensity = 0;
            const auto target = positions.find(led);
            if(target != positions.end()) for(const auto& wave : active)
            {
                const double distance=std::hypot(target->second.first-wave.x,target->second.second-wave.y);
                double value=RippleIntensity(wave.age,distance,wave.radius,duration,width);
                if((led==wave.origin || (wave.origin==54 && led==74)) && wave.age<hold) value=1;
                intensity=std::max(intensity,value);
            }
            const auto blend=[intensity](unsigned int a,unsigned int b){return static_cast<unsigned int>(std::lround(a+(static_cast<double>(b)-a)*intensity));};
            zone->SetLED(static_cast<int>(led),ToRGBColor(blend(RGBGetRValue(base),RGBGetRValue(purple)),blend(RGBGetGValue(base),RGBGetGValue(purple)),blend(RGBGetBValue(base),RGBGetBValue(purple))),Brightness,Temperature,Tint);
        }
    }
}
#endif
