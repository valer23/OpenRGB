/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "IcueLayers.h"
#include <stdexcept>

REGISTER_EFFECT(IcueLayers);

IcueLayers::IcueLayers() : RGBEffect()
{
    EffectDetails.EffectName = UI_Name();
    EffectDetails.EffectClassName = ClassName();
    EffectDetails.EffectDescription = "Lighting layers translated from iCUE XML. Load a translated profile; wave geometry and rain are adapted to OpenRGB zones.";
    EffectDetails.MinSpeed = 10;
    EffectDetails.MaxSpeed = 200;
    EffectDetails.SupportsRandom = false;
    EffectDetails.HasCustomSettings = true;
    SetSpeed(100);
    SetFPS(30);
}

namespace
{
double Bounded(const json& source, const char* key, double fallback, double low, double high)
{
    const double value = source.value(key, fallback);
    if(!std::isfinite(value) || value < low || value > high) throw std::runtime_error("Invalid lighting number");
    return value;
}
IcuePattern::Color ReadColor(const json& source)
{
    if(!source.is_array() || source.size() != 4) throw std::runtime_error("Invalid RGBA");
    IcuePattern::Color color{};
    for(unsigned int i = 0; i < 4; i++)
    {
        color[i] = source[i].get<double>();
        if(!std::isfinite(color[i]) || color[i] < 0 || color[i] > 255) throw std::runtime_error("Invalid channel");
    }
    return color;
}
}

void IcueLayers::LoadCustomSettings(json settings)
{
    // Parse once, outside frame rendering. Keep the prior settings if validation fails.
    try
    {
        if(settings.at("schema") != 1 || !settings.at("layers").is_array() || settings.at("layers").size() > 64) return;
        std::vector<IcuePattern::Layer> parsed;
        for(const auto& value : settings.at("layers"))
        {
            IcuePattern::Layer layer;
            layer.kind = value.at("kind").get<std::string>();
            if(layer.kind != "StaticLighting" && layer.kind != "GradientLighting" && layer.kind != "ColorWaveLighting" &&
               layer.kind != "WaveLighting" && layer.kind != "RainLighting" && layer.kind != "ColorShiftLighting" && layer.kind != "ColorPulseLighting") return;
            layer.period = Bounded(value, "period", 1, 0.01, 3600);
            layer.gain = Bounded(value, "gain", 1, 0, 1);
            layer.opacity = Bounded(value, "opacity", 1, 0, 1);
            layer.angle = Bounded(value, "angle", 0, -360, 360);
            layer.reverse = value.value("reverse", false);
            layer.vertical = value.value("vertical", false);
            layer.mask_extent = static_cast<unsigned int>(Bounded(value, "mask_extent", 0, 0, 10000));
            const auto& mask = value.at("mask");
            const auto& colors = value.at("colors");
            const auto& stops = value.at("stops");
            if(!mask.is_array() || mask.size() > 10000 || !colors.is_array() || colors.size() > 256 || !stops.is_array() || stops.size() > 1024) return;
            for(const auto& item : mask)
            {
                if(!item.is_number_integer() || item.get<long long>() < 0 || item.get<long long>() >= layer.mask_extent) return;
                layer.mask.push_back(item.get<unsigned int>());
            }
            for(const auto& item : colors) layer.colors.push_back(ReadColor(item));
            for(const auto& item : stops) layer.stops.push_back({Bounded(item, "time", 0, 0, 1), ReadColor(item.at("rgba"))});
            std::stable_sort(layer.stops.begin(), layer.stops.end(), [](const auto& a, const auto& b){return a.time < b.time;});
            parsed.push_back(std::move(layer));
        }
        layers = std::move(parsed);
        saved_settings = std::move(settings);
        seconds = 0;
    }
    catch(const std::exception&)
    {
        // Profiles are data, and a malformed profile must not terminate OpenRGB.
    }
}

json IcueLayers::SaveCustomSettings() { return saved_settings; }

void IcueLayers::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    for(ControllerZone* zone : controller_zones)
    {
        const unsigned int count = zone->leds_count();
        std::vector<std::array<double, 2>> positions(count);
        for(unsigned int led = 0; led < count; led++)
        {
            const double p = count > 1 ? static_cast<double>(led) / (count - 1) : 0.5;
            positions[led] = {p, p};
        }
        if(zone->type() == ZONE_TYPE_MATRIX || zone->type() == ZONE_TYPE_MATRIX_LOOP_X || zone->type() == ZONE_TYPE_MATRIX_LOOP_Y)
        {
            const unsigned int width = zone->matrix_map_width(), height = zone->matrix_map_height();
            for(unsigned int row = 0; row < height; row++)
            {
                for(unsigned int col = 0; col < width; col++)
                {
                    const unsigned int led = zone->map()[row * width + col];
                    if(led < count) positions[led] = {width > 1 ? static_cast<double>(col) / (width - 1) : 0.5,
                                                    height > 1 ? static_cast<double>(row) / (height - 1) : 0.5};
                }
            }
        }
        for(unsigned int led = 0; led < count; led++)
        {
            const auto color = IcuePattern::Sample(layers, seconds, zone->reverse ? 1 - positions[led][0] : positions[led][0], positions[led][1], led);
            zone->SetLED(static_cast<int>(led), ToRGBColor(static_cast<unsigned int>(std::lround(color[0])),
                static_cast<unsigned int>(std::lround(color[1])), static_cast<unsigned int>(std::lround(color[2]))), Brightness, Temperature, Tint);
        }
    }
    if(FPS) seconds += static_cast<double>(Speed) / (100.0 * FPS);
}
