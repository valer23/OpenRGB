#include "plugins/BatteryStatus/BatteryStatusPresentation.h"
#include <cassert>
#include <iostream>
int main()
{
    using nlohmann::json;
    json row={{"id","mouse"},{"name","Mouse"},{"percentage",19},{"charging",false},
        {"availability","active"},{"quality","current"},{"age_ms",0},{"detail",""}};
    BatteryAlertPolicy alerts;
    assert(!alerts.ShouldNotify(row, false));
    assert(alerts.ShouldNotify(row, true));
    assert(!alerts.ShouldNotify(row, true));
    row["percentage"]=25; assert(!alerts.ShouldNotify(row,true));
    row["percentage"]=20; assert(alerts.ShouldNotify(row,true));
    row["percentage"]=30; alerts.ShouldNotify(row,true);
    row["percentage"]=10; row["quality"]="stale"; assert(!alerts.ShouldNotify(row,true));
    assert(BatteryTitle(row).contains("last known"));
    row["quality"]="current"; row["charging"]=nullptr; assert(!alerts.ShouldNotify(row,true));
    row["charging"]=true; assert(!alerts.ShouldNotify(row,true));
    row["charging"]=false; row["availability"]="disconnected"; assert(!alerts.ShouldNotify(row,true));
    row["availability"]="receiver"; assert(!alerts.ShouldNotify(row,true));
    assert(BatteryAvailabilityText(row).contains("sleep state unknown"));
    row["percentage"]=nullptr; assert(BatteryTitle(row).contains("unknown"));
    std::cout<<"Presentation and discharge-alert tests passed\n";
}
