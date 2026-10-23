/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>

namespace AsusDeltaIIProtocol
{
using Packet = std::array<unsigned char, 64>;

// Owner-captured 0B05:1AFA firmware settings commands. These are not a
// software animation protocol; do not stream frames through Builtin().
inline std::optional<Packet> Builtin(unsigned int mode, unsigned int brightness,
    unsigned char red, unsigned char green, unsigned char blue)
{
    if(mode < 1 || mode > 5 || brightness > 100) { return {}; }
    Packet packet = {};
    packet[0] = 0xCC;
    packet[1] = 0x51;
    packet[2] = 0x28;
    packet[5] = static_cast<unsigned char>(mode);
    packet[6] = static_cast<unsigned char>(brightness);
    packet[7] = red;
    packet[8] = green;
    packet[9] = blue;
    return packet;
}

inline Packet Power(bool enabled)
{
    Packet packet = {};
    packet[0] = 0xCC;
    packet[1] = 0x51;
    packet[2] = 0x10;
    packet[5] = enabled ? 1 : 0;
    return packet;
}

// Captured Aura Sync enable/disable command; this is separate from LED power.
inline Packet SoftwareMode(bool enabled)
{
    Packet packet = {};
    packet[0] = 0xCC;
    packet[1] = 0x51;
    packet[2] = 0x33;
    packet[5] = enabled ? 1 : 0;
    return packet;
}

using RGB = std::array<unsigned char, 3>;

// Owner-captured software frames contain 14 RGB triplets. The capture proves
// uniform colour output only; separate physical LED addressing remains untested.
// This encoder neither opens the receiver nor establishes a safe streaming rate.
inline Packet SoftwareFrame(const std::array<RGB, 14>& colours)
{
    Packet packet = {};
    packet[0] = 0xCC;
    packet[1] = 0x51;
    packet[2] = 0x30;
    size_t offset = 5;
    for(const auto& colour : colours)
    {
        std::copy(colour.begin(), colour.end(), packet.begin() + offset);
        offset += colour.size();
    }
    return packet;
}

inline bool Acknowledged(const Packet& request, const unsigned char* data, size_t size)
{
    // Captured successful replies contain the echoed command and zero status
    // bytes. Software frames (0x30) have no captured acknowledgement. Fail closed on unknown statuses, truncation or unrelated replies.
    return data && size == Packet{}.size() && request[0] == 0xCC && request[1] == 0x51 &&
        (request[2] == 0x10 || request[2] == 0x28 || request[2] == 0x33) && data[0] == request[0] &&
        data[1] == request[1] && data[2] == request[2] &&
        std::all_of(data + 3, data + size, [](unsigned char byte) { return byte == 0; });
}
}
