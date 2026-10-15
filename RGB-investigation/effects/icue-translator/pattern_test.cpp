#include "IcuePattern.h"
#include <cassert>
#include <iostream>
#include <cstdlib>
#undef assert
#define assert(condition) do { if(!(condition)) { std::cerr << "FAILED: " << #condition << '\n'; std::exit(1); } } while(false)
using namespace IcuePattern;
int main()
{
    Layer background; background.kind = "StaticLighting";
    background.stops = {{0, {10, 20, 30, 255}}};
    Layer flash; flash.kind = "GradientLighting"; flash.period = 5;
    flash.stops = {{0, {255,255,255,0}}, {0.5,{255,255,255,128}}, {1,{255,255,255,0}}};
    auto start = Sample({background, flash}, 0, 0, 0, 0);
    auto peak = Sample({background, flash}, 2.5, 0, 0, 0);
    assert(start[0] == 10 && start[1] == 20);
    assert(peak[0] > 132 && peak[0] < 134);
    assert(Sample({background, flash}, 5, 0, 0, 0) == start);
    flash.mask = {1}; flash.mask_extent = 2;
    assert(Sample({background, flash}, 2.5, 0, 0, 0) == start);
    assert(Sample({background, flash}, 2.5, 1, 0, 0) == peak);
    Layer wave; wave.kind = "ColorWaveLighting"; wave.period = 2;
    wave.colors = {{87,87,87,255}, {155,155,155,255}};
    // Fully opaque iCUE colour waves must not gain an artificial blackout.
    assert(Sample({background,wave}, 0, 0, 0, 0)[0] == 87);
    assert(Sample({background,wave}, 1, 0, 0, 0)[0] == 155);
    Layer rain; rain.kind = "RainLighting"; rain.period = 4;
    rain.colors = {{100,100,100,255}, {100,100,100,255}};
    // At seed zero the rain cycle wraps at t=4. Both sides must approach black.
    const auto before = Sample({rain}, 4 - 0.00001, 0, 0, 0);
    const auto after = Sample({rain}, 4 + 0.00001, 0, 0, 0);
    assert(std::abs(before[0] - after[0]) < 0.01);
    for(unsigned int frame=0; frame<300; frame++)
    {
        for(unsigned int led=0; led<48; led++)
        {
            const auto c=Sample({background, wave, flash}, frame/30.0, led/47.0, 0, led);
            for(double channel:c) assert(std::isfinite(channel) && channel>=0 && channel<=255);
        }
    }
    std::cout << "Layer timing, ARGB blending, masks, wave motion and bounds passed\n";
}
