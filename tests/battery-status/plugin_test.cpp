#include "PluginAPIStub.h"
#include "OpenRGBBatteryStatusInterface.h"
#include "BatteryStatus/BatteryStatusManager.h"
#include <QApplication>
#include <QPluginLoader>
#include <QMenu>
#include <QAction>
#include <QPointer>
#include <cassert>
#include <iostream>
class Host : public PluginAPIStub, public OpenRGBBatteryStatusInterface
{
public:
    BatteryStatusManager cache;
    unsigned int GetBatteryStatusVersion() const override { return 1; }
    uint64_t AcquireBatteryMonitoring() override { return cache.Acquire(); }
    void ReleaseBatteryMonitoring(uint64_t token) override { cache.Release(token); }
    nlohmann::json GetBatterySnapshots() override { return cache.Snapshots(); }
};
int main(int argc, char** argv)
{
    QApplication app(argc,argv);
    assert(argc == 2);
    QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
    auto* object=loader.instance();
    if(!object) { std::cerr<<loader.errorString().toStdString()<<'\n'; return 1; }
    auto* plugin=qobject_cast<OpenRGBPluginInterface*>(object);
    assert(plugin && plugin->GetPluginAPIVersion()==5);
    PluginAPIStub old_host;
    plugin->Load(&old_host);
    assert(plugin->GetTrayMenu()->actions()[0]->text().contains("unavailable"));
    plugin->Unload();
    Host host;
    // This cast also has to work across the actual plugin DLL boundary.
    plugin->Load(&host);
    assert(host.cache.Enabled());
    QPointer<QMenu> menu=plugin->GetTrayMenu();
    assert(menu && menu->actions()[0]->text().contains("Dark Core"));
    auto* monitor=menu->findChild<QAction*>("BatteryMonitoring");
    assert(monitor && monitor->isChecked());
    monitor->setChecked(false); assert(!host.cache.Enabled());
    monitor->setChecked(true); assert(host.cache.Enabled());
    auto* widget=plugin->GetWidget(); assert(widget);
    plugin->OnProfileAboutToLoad(); plugin->OnProfileLoad({}); plugin->OnProfileSave();
    assert(host.cache.Enabled());
    delete widget; // The real host may destroy its page before unloading the plugin.
    plugin->Unload(); assert(!host.cache.Enabled());
    // PluginManager invokes Unload BEFORE the GUI removes this menu.
    assert(!menu.isNull());
    assert(menu->winId());
    assert(menu->menuAction());
    plugin->Unload(); // idempotent
    for(int i=0;i<3;++i) { plugin->Load(&host); assert(host.cache.Enabled()); plugin->Unload(); }
    assert(!host.cache.Enabled());
    QPointer<QMenu> final_menu = plugin->GetTrayMenu();
    assert(loader.unload());
    assert(final_menu.isNull());
    std::cout<<"Actual DLL: old host fallback, new host RTTI, toggles, profiles and unload passed\n";
}
