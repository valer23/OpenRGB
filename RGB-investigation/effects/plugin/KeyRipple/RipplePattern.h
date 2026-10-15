/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <cmath>

inline double RippleIntensity(double age, double distance, double radius, double duration, double width)
{
    if(age < 0 || age >= duration || duration <= 0 || width <= 0) return 0;
    const double difference = std::abs(distance - radius * age / duration);
    if(difference >= width) return 0;
    const double shape = 0.5 + 0.5 * std::cos(3.141592653589793 * difference / width);
    const double fade = std::min(1.0, (duration - age) / std::min(0.2, duration));
    return shape * fade;
}
