/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QObject>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QLabel>
#include <QMessageBox>
#include "OpenRGBPluginInterface.h"
#include "OpenRGBBatteryStatusInterface.h"
#include "BatteryStatusPresentation.h"

class BatteryStatusPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "BatteryStatusPlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)
public:
    ~BatteryStatusPlugin() override;
    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int GetPluginAPIVersion() override { return OPENRGB_PLUGIN_API_VERSION; }
    void Load(OpenRGBPluginAPIInterface* host) override;
    QWidget* GetWidget() override;
    QMenu* GetTrayMenu() override { return menu; }
    void Unload() override;
    void OnProfileAboutToLoad() override {}
    void OnProfileLoad(nlohmann::json) override {}
    nlohmann::json OnProfileSave() override { return nullptr; }
    unsigned char* OnSDKCommand(unsigned int, unsigned char*, unsigned int* size) override
        { if(size) { *size = 0; } return nullptr; }
    void ProfileManagerUpdated(unsigned int) override {}
    void ResourceManagerUpdated(unsigned int) override {}
    void SettingsManagerUpdated(unsigned int) override {}
private:
    void Refresh();
    void ConfirmAsusRefresh();
    void SelectAsusMode(bool cached);
    void MonitoringChanged(bool enabled);
    void SavePreferences();
    OpenRGBPluginAPIInterface* api = nullptr;
    OpenRGBBatteryStatusInterface* battery = nullptr;
    OpenRGBBatteryRefreshInterface* refresher = nullptr;
    OpenRGBBatteryModeInterface* modes = nullptr;
    uint64_t token = 0;
    bool notifications = false;
    QString refresh_error;
    QPointer<QMenu> menu;
    QPointer<QWidget> widget;
    QPointer<QLabel> summary;
    QPointer<QAction> monitoring_action;
    QPointer<QAction> notification_action;
    QPointer<QTimer> timer;
    QPointer<QAction> refresh_action;
    QPointer<QAction> cache_mode_action;
    QPointer<QAction> direct_mode_action;
    QPointer<QMessageBox> awake_confirmation;
    std::vector<QMenu*> device_menus;
    std::vector<QAction*> device_details;
    BatteryAlertPolicy alerts;
};
