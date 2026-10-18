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
