/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BrimstonePattern.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    bool temporal_change = false;
    bool spatial_change = false;
    const BrimstoneColor first = BrimstonePattern::Sample(0, 0, 0);
    for(unsigned int frame = 0; frame < 1000; frame++)
    {
        for(unsigned int led = 0; led < 192; led++)
        {
            const double time = frame / 10.0;
            const double position = led / 191.0;
            const BrimstoneColor color = BrimstonePattern::Sample(time, position, led);
            assert(std::isfinite(color.red) && std::isfinite(color.green));
            assert(color.red >= 0 && color.red <= 255);
            assert(color.green >= 0 && color.green <= 255);
            assert(color.blue == 0); // Source palette is entirely orange/red.
            const BrimstoneColor repeat = BrimstonePattern::Sample(time, position, led);
            assert(color.red == repeat.red && color.green == repeat.green);
            temporal_change |= frame > 0 && led == 0 && std::abs(color.red - first.red) > 1;
            spatial_change |= frame == 0 && led > 0 && std::abs(color.red - first.red) > 1;
        }
    }
    assert(temporal_change && spatial_change);
    std::cout << "Brimstone palette bounds, deterministic playback and spatial/temporal animation pass\n";
}
