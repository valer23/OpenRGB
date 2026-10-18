#include "BatteryStatus/AsusDeltaIIBattery.h"
#include <iostream>
#include <string>
int main(int argc,char** argv)
{
    if(argc==2 && std::string(argv[1])=="--observe")
    {
        auto result=AsusDeltaIIBattery::Observe();std::cout<<result.dump(2)<<'\n';return result.contains("error")?1:0;
    }
    if(argc==3 && std::string(argv[1])=="--awake-read" && std::string(argv[2])=="--confirm-headset-on")
    {
        auto result=AsusDeltaIIBattery::ReadAwakeSnapshot(true);std::cout<<result.dump(2)<<'\n';return result.contains("error")?1:0;
    }
    std::cerr<<"Use --observe (passive only), or --awake-read --confirm-headset-on after owner confirmation.\n";
    return 2;
}
