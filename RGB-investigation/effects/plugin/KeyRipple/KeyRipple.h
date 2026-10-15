/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#ifdef _WIN32
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "RawKeyboardInput.h"
#include "IcueLayers.h"
#include <memory>

class KeyRipple : public RGBEffect
{
    Q_OBJECT
public:
    KeyRipple();
    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new KeyRipple;});
    static std::string const ClassName() { return "KeyRipple"; }
    static std::string const UI_Name() { return QT_TR_NOOP("Keyboard Ripple"); }
    void StepEffect(std::vector<ControllerZone*>) override;
    void EffectState(bool enabled) override;
    void LoadCustomSettings(json settings) override;
    json SaveCustomSettings() override;
private:
    std::unique_ptr<IcueLayers> background_layers;
    json background_settings;
    std::string background_class;
    double background_seconds = 0;
    double background_speed = 100;
    RawKeyboardInput input;
    std::vector<RawKeyboardInput::Press> waves;
    double hold = 0.1;
    double duration = 0.8;
    double width = 1.8;
};
#endif
