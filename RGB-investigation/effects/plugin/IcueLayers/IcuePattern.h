/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace IcuePattern
{
using Color = std::array<double, 4>;
struct Stop { double time; Color color; };
struct Layer
{
    std::string kind;
    double period = 1, gain = 1, opacity = 1, angle = 0;
    bool reverse = false, vertical = false;
    unsigned int mask_extent = 0;
    std::vector<unsigned int> mask;
    std::vector<Stop> stops;
    std::vector<Color> colors;
};

inline double Fract(double value) { return value - std::floor(value); }
inline Color Mix(const Color& a, const Color& b, double amount)
{
    Color out{};
    for(unsigned int i = 0; i < 4; i++) out[i] = a[i] + (b[i] - a[i]) * amount;
    return out;
}
inline Color Timeline(const std::vector<Stop>& stops, double phase)
{
    if(stops.empty()) return {0, 0, 0, 0};
    if(phase <= stops.front().time) return stops.front().color;
    for(std::size_t i = 1; i < stops.size(); i++)
    {
        if(phase <= stops[i].time)
        {
            const double span = stops[i].time - stops[i - 1].time;
            return Mix(stops[i - 1].color, stops[i].color,
                       span > 0 ? (phase - stops[i - 1].time) / span : 1);
        }
    }
    return stops.back().color;
}
inline double Hash(std::uint32_t value)
{
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu; value ^= value >> 16;
    return static_cast<double>(value) / 4294967295.0;
}
inline Color Sample(const std::vector<Layer>& layers, double seconds, double x, double y, unsigned int led)
{
    Color result{0, 0, 0, 255};
    for(std::size_t index = 0; index < layers.size(); index++)
    {
        const Layer& layer = layers[index];
        if(layer.period <= 0) continue;
        if(!layer.mask.empty() && layer.mask_extent)
        {
            const unsigned int cell = static_cast<unsigned int>(std::lround(x * (layer.mask_extent - 1)));
            if(std::find(layer.mask.begin(), layer.mask.end(), cell) == layer.mask.end()) continue;
        }
        double position = layer.vertical ? y : x;
        if(layer.reverse) position = 1 - position;
        const double phase = Fract(seconds / layer.period);
        Color color{0, 0, 0, 0};
        if(layer.kind == "StaticLighting" || layer.kind == "GradientLighting")
        {
            color = Timeline(layer.stops, layer.kind == "StaticLighting" ? 0 : phase);
        }
        else if(layer.kind == "WaveLighting")
        {
            const double angle = layer.angle * 3.141592653589793 / 180.0;
            color = Timeline(layer.stops, Fract(phase - x * std::cos(angle) - y * std::sin(angle)));
        }
        else if(!layer.colors.empty())
        {
            const Color& a = layer.colors.front();
            const Color& b = layer.colors.back();
            const double wave = 0.5 - 0.5 * std::cos(6.283185307179586 * (phase - position));
            if(layer.kind == "ColorWaveLighting")
            {
                color = Mix(a, b, wave);
                // Keep the source palette's alpha. An opaque colour wave
                // interpolates colours; it is not a beam fading to black.
            }
            else if(layer.kind == "ColorShiftLighting")
            {
                color = Mix(a, b, 0.5 - 0.5 * std::cos(6.283185307179586 * phase));
            }
            else if(layer.kind == "ColorPulseLighting")
            {
                const double cycle = seconds / layer.period;
                color = static_cast<unsigned long long>(cycle) % 2 ? b : a;
                color[3] *= std::pow(std::sin(3.141592653589793 * phase), 2);
            }
            else if(layer.kind == "RainLighting")
            {
                const double offset = Hash(led * 2654435761u + static_cast<unsigned int>(index) * 97u);
                const double drop = Fract(phase + offset - position * 0.4);
                color = Mix(a, b, offset);
                // A continuous rise and fall avoids a jump at the cycle wrap.
                const double envelope = drop < 0.4 ? std::pow(std::sin(3.141592653589793 * drop / 0.4), 2) : 0;
                color[3] *= envelope;
            }
        }
        const double alpha = std::clamp(color[3] / 255.0 * layer.opacity, 0.0, 1.0);
        for(unsigned int channel = 0; channel < 3; channel++)
        {
            result[channel] = std::clamp(result[channel] * (1 - alpha) + color[channel] * layer.gain * alpha, 0.0, 255.0);
        }
    }
    return result;
}
}
