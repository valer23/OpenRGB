#include "BatteryStatus/AsusDeltaIIBattery.h"
#include <cassert>
#include <iostream>
int main()
{
    const auto result=AsusDeltaIIBattery::ReadAwakeSnapshot(false);
    assert(result["percentage"].is_null());
    assert(result["automatic_polling_enabled"]==false && result["no_wake_validated"]==false);
    assert(result["error"].get<std::string>().find("no interface opened")!=std::string::npos);
    std::cout<<"Delta II validation gate refuses acquisition without awake confirmation\n";
}
