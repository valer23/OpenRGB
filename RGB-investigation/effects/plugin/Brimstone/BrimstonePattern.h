/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

struct BrimstoneColor
{
    double red;
    double green;
    double blue;
};

// Palette is extracted from LG_Brimstone_lGoYlCr.cueprofile. The rain renderer
// is a deterministic adaptation: iCUE's random generator and layout units are
// not specified by its XML. No external code from the profile is executed.
class BrimstonePattern
{
public:
    static BrimstoneColor Sample(double seconds, double position, unsigned int seed)
    {
        BrimstoneColor result = {70, 24, 0}; // BrimstoneBG #FF461800
        const BrimstoneColor rain[4] = {{255, 56, 0}, {255, 58, 0}, {82, 34, 0}, {82, 23, 0}};
        for(unsigned int layer = 0; layer < 4; layer++)
        {
            const double rate = layer < 2 ? 2.0 : 1.0;
            const double time = seconds * rate + position * 3.0;
            const std::uint32_t cycle = (std::uint32_t)std::floor(time);
            const double phase = time - std::floor(time);
            const double random = Hash(seed * 17u + layer * 131u + cycle * 997u);
            const double peak = 0.15 + random * 0.55;
            const double intensity = std::max(0.0, 1.0 - std::abs(phase - peak) / 0.20);
            result = Blend(result, rain[layer], intensity * (layer < 2 ? 0.60 : 0.20));
        }
        // Two opposite moving orange waves; 1.4/1.2 s periods come from the
        // exported keyboard WaveLighting layers, adapted to a normalized zone.
        result = Wave(result, {255, 43, 0}, seconds / 1.4 + position, 0.42);
        result = Wave(result, {255, 70, 0}, seconds / 1.2 - position, 0.35);
        return result;
    }

private:
    static double Hash(std::uint32_t value)
    {
        value ^= value >> 16;
        value *= 0x7FEB352Du;
        value ^= value >> 15;
        value *= 0x846CA68Bu;
        value ^= value >> 16;
        return (value & 0xFFFFFFu) / 16777215.0;
    }

    static BrimstoneColor Blend(BrimstoneColor base, BrimstoneColor top, double opacity)
    {
        return {base.red + (top.red - base.red) * opacity,
                base.green + (top.green - base.green) * opacity,
                base.blue + (top.blue - base.blue) * opacity};
    }

    static BrimstoneColor Wave(BrimstoneColor base, BrimstoneColor top, double phase, double strength)
    {
        phase -= std::floor(phase);
        // Bright leading edge with an orange tail fading to transparent.
        const double opacity = phase < 0.30 ? (1.0 - phase / 0.30) * strength : 0.0;
        return Blend(base, top, opacity);
    }
};
