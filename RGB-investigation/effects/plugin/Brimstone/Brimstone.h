/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RGBEffect.h"
#include "EffectRegisterer.h"

class Brimstone : public RGBEffect
{
    Q_OBJECT
public:
    Brimstone();
    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new Brimstone;});
    static std::string const ClassName() { return "Brimstone"; }
    static std::string const UI_Name() { return QT_TR_NOOP("Brimstone"); }
    void StepEffect(std::vector<ControllerZone*>) override;

private:
    double seconds = 0.0;
};
