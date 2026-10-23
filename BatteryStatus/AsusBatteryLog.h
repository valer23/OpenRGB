/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

// Reads existing ASUS diagnostic broadcasts only. No device, IPC or vendor calls.
class AsusBatteryLog
{
public:
    static nlohmann::json Read();
    static nlohmann::json ReadDirectory(const std::filesystem::path& directory, uint64_t now_ms);
    static nlohmann::json Parse(const std::string& text, uint64_t now_ms);
};
