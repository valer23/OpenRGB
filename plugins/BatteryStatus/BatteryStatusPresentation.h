/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QString>
#include <map>
#include <nlohmann/json.hpp>

inline QString BatteryTitle(const nlohmann::json& row)
{
    QString value = row["percentage"].is_number_integer() ?
        QString::number(row["percentage"].get<int>()) + "%" : "unknown";
    if(row["percentage"].is_number_integer() && row["quality"] != "current")
    {
        value += " (last known)";
    }
    if(row["quality"] == "current" && row["charging"] == true) { value += " - charging"; }
    return QString::fromStdString(row["name"].get<std::string>()) + ": " + value;
}

inline QString BatteryAvailabilityText(const nlohmann::json& row)
{
    if(row["availability"] == "asus_cache" || row["availability"] == "asus_cache_offline")
    {
        const QString charging = row["charging"].is_boolean() ?
            (row["charging"].get<bool>() ? "charging in that report" : "not charging in that report") :
            "charging status unknown";
        return "ASUS cached report; " + charging +
            (row["availability"] == "asus_cache_offline" ? "; ASUS reports device unavailable" : "; headset sleep state unknown");
    }
    if(row["availability"] == "manual")
    {
        const QString charging = row["charging"].is_boolean() ?
            (row["charging"].get<bool>() ? "charging at last refresh" : "not charging at last refresh") :
            "charging status unknown";
        return "Manual reading; " + charging + "; refresh again while the headset is awake";
    }
    if(row["availability"] == "refreshing") { return "Reading headset battery once"; }
    if(row["availability"] == "receiver") { return "Receiver reachable; headset sleep state unknown"; }
    return QString::fromStdString(row["availability"].get<std::string>());
}
class BatteryAlertPolicy
{
public:
    bool ShouldNotify(const nlohmann::json& row, bool enabled)
    {
        if(row["quality"] != "current" || row["availability"] != "active" ||
           !row["percentage"].is_number_integer() || !row["charging"].is_boolean()) { return false; }
        const auto id = row["id"].get<std::string>();
        const int level = row["percentage"].get<int>();
        if(level >= 25) { notified[id] = false; }
        if(!enabled || row["charging"].get<bool>() || level > 20 || notified[id]) { return false; }
        notified[id] = true;
        return true;
    }
private:
    std::map<std::string, bool> notified;
};
