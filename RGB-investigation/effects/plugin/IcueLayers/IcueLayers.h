/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "IcuePattern.h"

class IcueLayers : public RGBEffect
{
    Q_OBJECT
public:
    IcueLayers();
    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new IcueLayers;});
    static std::string const ClassName() { return "IcueLayers"; }
    static std::string const UI_Name() { return QT_TR_NOOP("Imported iCUE layers"); }
    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json settings) override;
    json SaveCustomSettings() override;
private:
    double seconds = 0;
    std::vector<IcuePattern::Layer> layers;
    json saved_settings;
};
