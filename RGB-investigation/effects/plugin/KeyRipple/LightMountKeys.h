/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <array>

// Physical Set-1 scan codes, not localized character codes. Lamp IDs are
// specific to the 135-lamp Light Mount layout and must not be used elsewhere.
inline int LightMountLamp(unsigned int scan)
{
    static const std::array<int, 512> lamps = []() {
        std::array<int, 512> result{};
        result.fill(-1);
        result[0x01] = 108; // Escape
        result[0x29] = 20; // backquote
        for(unsigned int code = 0x02; code <= 0x0D; code++) result[code] = 21 + code - 0x02;
        result[0x0E] = 33; result[0x0F] = 41; // Backspace, Tab
        for(unsigned int code = 0x10; code <= 0x1B; code++) result[code] = 42 + code - 0x10;
        result[0x1C] = 54; result[0x1D] = 95; // Enter, left Control
        for(unsigned int code = 0x1E; code <= 0x28; code++) result[code] = 63 + code - 0x1E;
        result[0x2A] = 78; result[0x2B] = 133; // left Shift, ISO backslash
        for(unsigned int code = 0x2C; code <= 0x35; code++) result[code] = 79 + code - 0x2C;
        result[0x36] = 89; result[0x37] = 39; result[0x38] = 97;
        result[0x39] = 98; result[0x3A] = 62;
        for(unsigned int code = 0x3B; code <= 0x44; code++) result[code] = code - 0x3B;
        result[0x57] = 10; result[0x58] = 11;
        result[0x45] = 37; result[0x46] = 13;
        result[0x47] = 58; result[0x48] = 59; result[0x49] = 60; result[0x4A] = 40;
        result[0x4B] = 75; result[0x4C] = 76; result[0x4D] = 77; result[0x4E] = 61;
        result[0x4F] = 91; result[0x50] = 92; result[0x51] = 93; result[0x52] = 106; result[0x53] = 107;
        result[0x56] = 132;
        result[0x11C] = 94; result[0x11D] = 102; result[0x135] = 38;
        result[0x137] = 12; result[0x138] = 99;
        result[0x147] = 35; result[0x148] = 90; result[0x149] = 36;
        result[0x14B] = 103; result[0x14D] = 105; result[0x14F] = 56;
        result[0x150] = 104; result[0x151] = 57; result[0x152] = 34; result[0x153] = 55;
        result[0x15B] = 96; result[0x15C] = 100; result[0x15D] = 101;
        return result;
    }();
    return scan < lamps.size() ? lamps[scan] : -1;
}
