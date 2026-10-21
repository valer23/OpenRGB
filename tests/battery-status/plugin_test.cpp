#include "PluginAPIStub.h"
#include "OpenRGBBatteryStatusInterface.h"
#include "BatteryStatus/BatteryStatusManager.h"
#include <QApplication>
#include <QPluginLoader>
#include <QMenu>
#include <QAction>
#include <QPointer>
#include <QMessageBox>
#include <QPushButton>
#include <cassert>
#include <iostream>
class LegacyHost : public PluginAPIStub, public OpenRGBBatteryStatusInterface
{
public:
    BatteryStatusManager cache;
    unsigned int GetBatteryStatusVersion() const override { return 1; }
    uint64_t AcquireBatteryMonitoring() override { return cache.Acquire(); }
    void ReleaseBatteryMonitoring(uint64_t token) override { cache.Release(token); }
    nlohmann::json GetBatterySnapshots() override { return cache.Snapshots(); }
};
class Host : public LegacyHost, public OpenRGBBatteryRefreshInterface
{
public:
    int requests=0;
    bool confirmed=false;
    unsigned int GetBatteryRefreshVersion() const override { return 1; }
    bool RequestAsusBatteryRefresh(uint64_t token, bool awake) override
    { assert(token != 0); ++requests; confirmed=awake; return true; }
};
class ModeHost : public Host, public OpenRGBBatteryModeInterface
{
public:
    unsigned int GetBatteryModeVersion() const override { return 1; }
    bool SetAsusBatteryMode(const std::string& mode) override
    {
        assert(mode=="cache" || mode=="direct");
        cache.SetAsusCacheMode(mode=="cache");return true;
    }
};
int main(int argc, char** argv)
{
    QApplication app(argc,argv);
    assert(argc == 2 || argc == 3);
    if(argc == 3)
    {
        QPluginLoader existing_effects(QString::fromLocal8Bit(argv[2]));
        const auto metadata = existing_effects.metaData()["MetaData"].toObject();
        assert(metadata["OpenRGBPluginAPIVersion"].toInt() == 5);
        auto* effects = qobject_cast<OpenRGBPluginInterface*>(existing_effects.instance());
        assert(effects && effects->GetPluginAPIVersion() == 5);
        assert(!effects->GetPluginInfo().Name.empty());
        assert(existing_effects.unload());
        std::cout<<"Installed effects DLL: API-v5 metadata, load, interface cast and unload passed (no hardware host supplied)\n";
    }
    QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
    // PluginManager rejects a DLL without these discovery fields before Load().
    const auto battery_metadata = loader.metaData()["MetaData"].toObject();
    assert(battery_metadata["OpenRGBPluginAPIVersion"].toInt() == 5);
    assert(battery_metadata["Id"].toString() == "org.openrgb.battery-status");
    assert(!battery_metadata["VersionStr"].toString().isEmpty());
    auto* object=loader.instance();
    if(!object) { std::cerr<<loader.errorString().toStdString()<<'\n'; return 1; }
    auto* plugin=qobject_cast<OpenRGBPluginInterface*>(object);
    assert(plugin && plugin->GetPluginAPIVersion()==5);
    PluginAPIStub old_host;
    plugin->Load(&old_host);
    assert(plugin->GetTrayMenu()->actions()[0]->text().contains("unavailable"));
    plugin->Unload();
    LegacyHost legacy;
    plugin->Load(&legacy);
    assert(!plugin->GetTrayMenu()->findChild<QAction*>("BatteryAsusRefresh"));
    plugin->Unload();
    Host host;
    // This cast also has to work across the actual plugin DLL boundary.
    plugin->Load(&host);
    assert(host.cache.Enabled());
    QPointer<QMenu> menu=plugin->GetTrayMenu();
    assert(menu && menu->actions()[0]->text().contains("Dark Core"));
    auto* monitor=menu->findChild<QAction*>("BatteryMonitoring");
    assert(monitor && monitor->isChecked());
    host.cache.Publish(BatteryStatusManager::CorsairId,76,0,"active");
    monitor->setChecked(false); assert(!host.cache.Enabled());
    assert(menu->actions()[0]->text().contains("last known"));
    monitor->setChecked(true); assert(host.cache.Enabled());
    auto* refresh=menu->findChild<QAction*>("BatteryAsusRefresh");
    assert(refresh && refresh->isEnabled());
    refresh->trigger();
    auto* confirm=menu->findChild<QMessageBox*>("BatteryAwakeConfirmation");
    assert(confirm && confirm->defaultButton()==confirm->button(QMessageBox::Cancel));
    assert(host.requests==0);
    confirm->done(QMessageBox::Cancel);
    assert(host.requests==0);
    refresh->trigger();
    confirm=menu->findChild<QMessageBox*>("BatteryAwakeConfirmation");
    assert(confirm);
    confirm->done(QMessageBox::Yes);
    assert(host.requests==1 && host.confirmed);
    monitor->setChecked(false); assert(!refresh->isEnabled());
    monitor->setChecked(true);
    auto* widget=plugin->GetWidget(); assert(widget);
    plugin->OnProfileAboutToLoad(); plugin->OnProfileLoad({}); plugin->OnProfileSave();
    assert(host.cache.Enabled());
    assert(host.cache.Snapshots()[0]["percentage"]==76);
    delete widget; // The real host may destroy its page before unloading the plugin.
    refresh->trigger(); // Unload with a confirmation outstanding must cancel it.
    plugin->Unload(); assert(!host.cache.Enabled());
    assert(host.requests==1);
    // PluginManager invokes Unload BEFORE the GUI removes this menu.
    assert(!menu.isNull());
    assert(menu->winId());
    assert(menu->menuAction());
    plugin->Unload(); // idempotent
    for(int i=0;i<3;++i) { plugin->Load(&host); assert(host.cache.Enabled()); plugin->Unload(); }
    assert(!host.cache.Enabled());
    Host cached_host;cached_host.cache.SetAsusCacheMode(true);
    plugin->Load(&cached_host);
    auto* cached_refresh=plugin->GetTrayMenu()->findChild<QAction*>("BatteryAsusRefresh");
    assert(cached_refresh && !cached_refresh->isVisible() && !cached_refresh->isEnabled());
    cached_refresh->trigger();assert(cached_host.requests==0);
    plugin->Unload();
    ModeHost modes;
    modes.preferences["unrelated"]=42;
    plugin->Load(&modes);
    auto* mode_menu=plugin->GetTrayMenu();
    auto* cache_mode=mode_menu->findChild<QAction*>("BatteryAsusCacheMode");
    auto* direct_mode=mode_menu->findChild<QAction*>("BatteryAsusDirectMode");
    auto* mode_refresh=mode_menu->findChild<QAction*>("BatteryAsusRefresh");
    assert(cache_mode && direct_mode && cache_mode->isChecked() && !direct_mode->isChecked());
    assert(!mode_refresh->isVisible() && modes.requests==0);
    direct_mode->trigger();
    assert(direct_mode->isChecked() && !cache_mode->isChecked());
    assert(mode_refresh->isVisible() && mode_refresh->isEnabled() && modes.requests==0);
    assert(modes.preferences["asus_mode"]=="direct" && modes.preferences["unrelated"]==42);
    mode_refresh->trigger();
    assert(mode_menu->findChild<QMessageBox*>("BatteryAwakeConfirmation"));
    cache_mode->trigger(); // Switching invalidates an outstanding awake confirmation.
    assert(!mode_menu->findChild<QMessageBox*>("BatteryAwakeConfirmation"));
    assert(modes.requests==0 && !mode_refresh->isVisible());
    direct_mode->trigger();
    plugin->Unload();plugin->Load(&modes);
    assert(plugin->GetTrayMenu()->findChild<QAction*>("BatteryAsusDirectMode")->isChecked());
    assert(modes.requests==0); // Persisted direct selection never performs a read.
    plugin->Unload();
    modes.preferences["asus_mode"]="invalid";
    plugin->Load(&modes);
    assert(plugin->GetTrayMenu()->findChild<QAction*>("BatteryAsusCacheMode")->isChecked());
    plugin->Unload();
    QPointer<QMenu> final_menu = plugin->GetTrayMenu();
    assert(loader.unload());
    assert(final_menu.isNull());
    std::cout<<"Actual DLL: old host fallback, new host RTTI, toggles, profiles and unload passed\n";
}
