/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <cstddef>
#include <optional>

namespace BatteryProtocol
{
inline std::optional<int> CorsairLevel(const unsigned char* data, size_t size)
{
    if(!data || size < 5 || data[0] != 1 || data[1] != 2 || data[2] != 0) { return {}; }
    const unsigned int value = data[3] | (data[4] << 8);
    if(value > 1000) { return {}; }
    return value / 10;
}
inline bool AsusReply(const unsigned char* data, size_t size, unsigned char key)
{
    return data && size >= 6 && data[0] == 0xCC && data[1] == 0x12 && data[2] == key &&
        data[3] == 0 && data[4] == 0 && !(size >= 7 && data[5] == 0xFF && data[6] == 0xAA);
}
inline std::optional<int> AsusLevel(const unsigned char* data, size_t size)
{
    if(size < 7 || !AsusReply(data, size, 7) || data[6] > 100) { return {}; }
    return data[6];
}
inline std::optional<int> AsusCharging(const unsigned char* data, size_t size)
{
    if(!AsusReply(data, size, 8) || data[5] > 1) { return {}; }
    return data[5];
}
}
