/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BatteryStatusPlugin.h"
#include <QApplication>
#include <QSystemTrayIcon>
#include <QVBoxLayout>

OpenRGBPluginInfo BatteryStatusPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info = {};
    info.Name = "Battery Status";
    info.Description = "Cached wireless-device battery status in the existing tray icon";
    info.Version = "0.1.0";
    info.Location = OPENRGB_PLUGIN_LOCATION_SETTINGS;
    info.Label = "Battery Status";
    return info;
}

void BatteryStatusPlugin::Load(OpenRGBPluginAPIInterface* host)
{
    Unload();
    if(menu) { delete menu; }
    if(widget) { delete widget; }
    api = host;
    battery = dynamic_cast<OpenRGBBatteryStatusInterface*>(host);
    if(battery && battery->GetBatteryStatusVersion() != 1) { battery = nullptr; }
    refresher = dynamic_cast<OpenRGBBatteryRefreshInterface*>(host);
    if(refresher && refresher->GetBatteryRefreshVersion() != 1) { refresher = nullptr; }
    menu = new QMenu("Battery Status");
    menu->setObjectName("BatteryStatusMenu");
    if(!battery)
    {
        menu->addAction("Battery support is unavailable in this OpenRGB build")->setEnabled(false);
        return;
    }
    auto settings = api->GetSettings("BatteryStatus");
    const bool monitoring = !settings.is_object() || !settings.contains("enabled") ||
        !settings["enabled"].is_boolean() || settings["enabled"].get<bool>();
    notifications = settings.is_object() && settings.contains("notifications") &&
        settings["notifications"].is_boolean() && settings["notifications"].get<bool>();
    for(const auto* name : {"Corsair Dark Core RGB Pro SE", "ASUS ROG Delta II"})
    {
        auto* entry = menu->addMenu(QString::fromUtf8(name) + ": unknown");
        device_menus.push_back(entry);
        auto* details = entry->addAction("Waiting for cached data");
        details->setEnabled(false);
        device_details.push_back(details);
    }
    if(refresher)
    {
        refresh_action = device_menus[1]->addAction("Refresh headset battery while awake...");
        refresh_action->setObjectName("BatteryAsusRefresh");
        connect(refresh_action, &QAction::triggered, this, [this] { ConfirmAsusRefresh(); });
    }
    menu->addSeparator();
    monitoring_action = menu->addAction("Enable battery monitoring");
    monitoring_action->setObjectName("BatteryMonitoring");
    monitoring_action->setCheckable(true);
    monitoring_action->setChecked(monitoring);
    notification_action = menu->addAction("Low-battery notifications (20%)");
    notification_action->setObjectName("BatteryNotifications");
    notification_action->setCheckable(true);
    notification_action->setChecked(notifications);
    connect(monitoring_action, &QAction::toggled, this, [this](bool enabled) {
        MonitoringChanged(enabled); SavePreferences();
    });
    connect(notification_action, &QAction::toggled, this, [this](bool enabled) {
        notifications = enabled; SavePreferences();
    });
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] { Refresh(); });
    timer->start(1000);
    MonitoringChanged(monitoring);
}

QWidget* BatteryStatusPlugin::GetWidget()
{
    if(!widget)
    {
        widget = new QWidget;
        auto* layout = new QVBoxLayout(widget);
        summary = new QLabel;
        summary->setWordWrap(true);
        layout->addWidget(summary);
        layout->addWidget(new QLabel("Monitoring and notification options are in the Battery Status tray submenu."));
        layout->addStretch();
        Refresh();
    }
    return widget;
}

void BatteryStatusPlugin::MonitoringChanged(bool enabled)
{
    if(battery)
    {
        if(enabled && !token) { token = battery->AcquireBatteryMonitoring(); }
        else if(!enabled && token) { battery->ReleaseBatteryMonitoring(token); token = 0; }
    }
    Refresh();
}

void BatteryStatusPlugin::SavePreferences()
{
    if(api && monitoring_action)
    {
        api->SetSettings("BatteryStatus", {{"enabled", monitoring_action->isChecked()}, {"notifications", notifications}});
        api->SaveSettings();
    }
}

void BatteryStatusPlugin::Refresh()
{
    if(!battery)
    {
        if(summary) { summary->setText("This OpenRGB build does not provide battery telemetry. Install the matching fork build."); }
        return;
    }
    const auto rows = battery->GetBatterySnapshots();
    if(refresh_action)
    {
        refresh_action->setEnabled(token && !awake_confirmation && rows.size() > 1 && rows[1]["availability"] != "refreshing");
    }
    QStringList lines;
    for(size_t i = 0; i < device_menus.size() && i < rows.size(); ++i)
    {
        auto row = rows[i];
        if(!token && row["percentage"].is_number_integer()) { row["quality"] = "last_known"; }
        const QString title = BatteryTitle(row);
        device_menus[i]->setTitle(title);
        QString detail = token ? BatteryAvailabilityText(row) : "Monitoring disabled";
        if(row["age_ms"].is_number_unsigned() || row["age_ms"].is_number_integer())
        {
            detail += QString("; last updated %1 s ago").arg(row["age_ms"].get<uint64_t>() / 1000);
        }
        if(row["quality"] == "current" && row["charging"].is_null()) { detail += "; charging status unknown"; }
        const auto description = QString::fromStdString(row["detail"].get<std::string>());
        if(!description.isEmpty()) { detail += "; " + description; }
        if(i == 1 && !refresh_error.isEmpty()) { detail += "; " + refresh_error; }
        device_details[i]->setText(detail);
        lines << title + "\n" + detail;
        if(token && alerts.ShouldNotify(row, notifications))
        {
            // Reuse the host's tray object; never create a second tray icon.
            for(auto* top : QApplication::topLevelWidgets())
            {
                auto* tray = top->findChild<QSystemTrayIcon*>();
                if(tray && tray->isVisible())
                {
                    tray->showMessage("Low battery", title, QSystemTrayIcon::Warning);
                    break;
                }
            }
        }
    }
    if(summary) { summary->setText(lines.join("\n\n")); }
}

void BatteryStatusPlugin::Unload()
{
    if(awake_confirmation) { awake_confirmation->disconnect(this); delete awake_confirmation; }
    if(refresh_action) { refresh_action->disconnect(this); refresh_action->setEnabled(false); }
    if(timer) { timer->stop(); delete timer; }
    if(token && battery) { battery->ReleaseBatteryMonitoring(token); }
    token = 0;
    battery = nullptr;
    refresher = nullptr;
    api = nullptr;
    device_menus.clear();
    device_details.clear();
    // The core removes its stored traymenu pointer after Unload returns.
    // Keep Qt objects valid until the plugin root is destroyed by QPluginLoader.
    if(menu) { menu->disconnect(this); }
    if(monitoring_action) { monitoring_action->disconnect(this); }
    if(notification_action) { notification_action->disconnect(this); }
}

BatteryStatusPlugin::~BatteryStatusPlugin()
{
    Unload();
    if(menu) { delete menu; }
    if(widget) { delete widget; }
}

void BatteryStatusPlugin::ConfirmAsusRefresh()
{
    if(!refresher || !token || awake_confirmation) { return; }
    refresh_error.clear();
    awake_confirmation = new QMessageBox(QMessageBox::Question, "Refresh headset battery",
        "Confirm that the Delta II is powered on and awake. Read its battery and charging status once?\n\n"
        "If ASUS is using the receiver, the refresh will report unavailable. No ASUS component will be stopped.",
        QMessageBox::Yes | QMessageBox::Cancel, menu);
    awake_confirmation->setObjectName("BatteryAwakeConfirmation");
    awake_confirmation->setDefaultButton(QMessageBox::Cancel);
    awake_confirmation->setAttribute(Qt::WA_DeleteOnClose);
    connect(awake_confirmation, &QMessageBox::finished, this, [this](int result) {
        awake_confirmation = nullptr;
        if(result == QMessageBox::Yes && refresher && token)
        {
            if(!refresher->RequestAsusBatteryRefresh(token, true))
            {
                refresh_error = "Refresh could not start: monitoring may be paused or a read is already running";
            }
        }
        Refresh();
    });
    awake_confirmation->open();
    Refresh();
}
