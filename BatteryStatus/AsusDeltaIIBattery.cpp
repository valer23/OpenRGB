/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "AsusDeltaIIBattery.h"
#include "BatteryProtocol.h"
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIReceiver.h"
#include <array>
#include <chrono>
#ifdef _WIN32
namespace
{
using Clock = std::chrono::steady_clock;
using Handle = AsusDeltaIIReceiver::Transaction;
int Transfer(Handle& device, bool write, unsigned char* buffer, unsigned int size, unsigned int timeout)
{
    return device.Transfer(write, buffer, size, timeout);
}
bool Query(Handle& device, unsigned char key, std::array<unsigned char,64>& reply, std::string& error)
{
    const auto deadline = Clock::now() + std::chrono::milliseconds(250);
    std::array<unsigned char,64> request = {};
    request[0] = 0xCC; request[1] = 0x12; request[2] = key;
    if(Transfer(device, true, request.data(), request.size(), 100) != 64)
    {
        error = "Battery request did not complete"; return false;
    }
    for(unsigned int i = 0; i < 4 && Clock::now() < deadline; ++i)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if(remaining <= 0) { break; }
        const int count = Transfer(device, false, reply.data(), reply.size(), static_cast<unsigned int>(remaining));
        if(count < 0) { error = "Battery response transport failed"; return false; }
        if(count == 0) { break; }
        if(BatteryProtocol::AsusReply(reply.data(), static_cast<size_t>(count), key))
        {
            if(key == 7 && count < 7) { continue; }
            return true;
        }
        if(count >= 3 && reply[0] == 0xCC && reply[1] == 0xFF && reply[2] == 0xAA)
        {
            error = "Headset refused the request"; return false;
        }
        // Other events are not treated as a matching battery response.
    }
    error = "No valid matching battery reply before deadline";
    return false;
}
}
#endif

nlohmann::json AsusDeltaIIBattery::ReadAwakeSnapshot(bool owner_confirmed_awake)
{
    nlohmann::json result = {{"device", "ASUS ROG Delta II"}, {"percentage", nullptr}, {"charging", nullptr},
        {"automatic_polling_enabled", false}, {"no_wake_validated", false}};
    if(!owner_confirmed_awake)
    {
        result["error"] = "Owner confirmation that the headset is awake is required; no interface opened";
        return result;
    }
#ifdef _WIN32
    std::string error;
    auto handle = GetAsusDeltaIIReceiver().Acquire(error);
    if(!handle) { result["error"] = error; return result; }
    std::array<unsigned char,64> reply = {};
    if(!Query(handle, 7, reply, error)) { result["error"] = error; return result; }
    const auto percentage = BatteryProtocol::AsusLevel(reply.data(), reply.size());
    if(!percentage) { result["error"] = "Invalid battery percentage reply"; return result; }
    result["percentage"] = *percentage;
    result["level_report_prefix"] = std::vector<unsigned char>(reply.begin(), reply.begin() + 12);
    if(Query(handle, 8, reply, error))
    {
        const auto charging = BatteryProtocol::AsusCharging(reply.data(), reply.size());
        if(charging) { result["charging"] = *charging == 1; }
        result["charging_report_prefix"] = std::vector<unsigned char>(reply.begin(), reply.begin() + 12);
    }
    else { result["charging_error"] = error; }
    result["firmware_usb_version"] = "Not queried: no unrelated commands";
#else
    result["error"] = "Owner-hardware validation is currently Windows-only";
#endif
    return result;
}

nlohmann::json AsusDeltaIIBattery::ReadAuthorizedSleepTest(bool owner_authorized_one_query)
{
    nlohmann::json result = {{"device", "ASUS ROG Delta II"}, {"percentage", nullptr}, {"charging", nullptr},
        {"sleep_safety_experiment", true}, {"level_query_attempted", false},
        {"automatic_polling_enabled", false}, {"no_wake_validated", false}};
    if(!owner_authorized_one_query)
    {
        result["error"] = "Explicit authorization for one asleep battery query is required; no interface opened";
        return result;
    }
#ifdef _WIN32
    std::string error;
    auto handle = GetAsusDeltaIIReceiver().Acquire(error);
    if(!handle) { result["error"] = error; return result; }
    std::array<unsigned char,64> reply = {};
    result["level_query_attempted"] = true;
    // Exactly one documented battery request. Do not query charging or retry.
    if(!Query(handle, 7, reply, error)) { result["error"] = error; return result; }
    const auto percentage = BatteryProtocol::AsusLevel(reply.data(), reply.size());
    if(!percentage) { result["error"] = "Invalid battery percentage reply"; return result; }
    result["percentage"] = *percentage;
    result["level_report_prefix"] = std::vector<unsigned char>(reply.begin(), reply.begin() + 12);
    result["observation_required"] = "Owner must report whether the naturally asleep headset woke; a reply does not prove no-wake";
#else
    result["error"] = "Owner-hardware validation is currently Windows-only";
#endif
    return result;
}
nlohmann::json AsusDeltaIIBattery::Observe()
{
    nlohmann::json result = {{"device", "ASUS ROG Delta II"}, {"reports", nlohmann::json::array()},
        {"commands_sent", 0}, {"automatic_polling_enabled", false}, {"no_wake_validated", false}};
#ifdef _WIN32
    std::string error;
    auto handle = GetAsusDeltaIIReceiver().Acquire(error);
    if(!handle) { result["error"] = error; return result; }
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    for(unsigned int i = 0; i < 16 && Clock::now() < deadline; ++i)
    {
        std::array<unsigned char,64> report = {};
        const int count = Transfer(handle, false, report.data(), report.size(), 100);
        if(count < 0) { result["error"] = "Passive input unavailable"; break; }
        if(count > 0)
        {
            result["reports"].push_back(std::vector<unsigned char>(report.begin(), report.begin() + std::min(count,16)));
        }
    }
#else
    result["error"] = "Owner-hardware validation is currently Windows-only";
#endif
    return result;
}

nlohmann::json AsusDeltaIIBattery::InspectAvailability()
{
    nlohmann::json result = {{"device", "ASUS ROG Delta II"}, {"commands_sent", 0},
        {"automatic_polling_enabled", false}};
#ifdef _WIN32
    std::string error;
    auto handle = GetAsusDeltaIIReceiver().Acquire(error);
    result["exclusive_interface_available"] = static_cast<bool>(handle);
    if(!error.empty()) { result["error"] = error; }
#else
    result["error"] = "Interface inspection is currently Windows-only";
#endif
    return result;
}

nlohmann::json AsusDeltaIIBattery::ReadValidatedReceiverSnapshot(bool sleep_safe_level_query_validated)
{
    nlohmann::json result = {{"device", "ASUS ROG Delta II"}, {"percentage", nullptr},
        {"charging", nullptr}, {"availability", "unavailable"}};
    if(!sleep_safe_level_query_validated)
    {
        result["error"] = "Automatic queries disabled: sleep-safe level acquisition not validated; no interface opened";
        return result;
    }
#ifdef _WIN32
    std::string error;
    auto handle = GetAsusDeltaIIReceiver().Acquire(error);
    if(!handle)
    {
        if(error == "Exact Delta II vendor collection not present") { result["availability"] = "disconnected"; }
        result["error"] = error;
        return result;
    }
    std::array<unsigned char,64> reply = {};
    // Validation applies only to this level request, never to charging/settings.
    if(!Query(handle, 7, reply, error)) { result["error"] = error; return result; }
    const auto percentage = BatteryProtocol::AsusLevel(reply.data(), reply.size());
    if(!percentage) { result["error"] = "Invalid battery percentage reply"; return result; }
    result["percentage"] = *percentage;
    result["availability"] = "receiver";
#else
    result["error"] = "Delta II acquisition is currently Windows-only";
#endif
    return result;
}
