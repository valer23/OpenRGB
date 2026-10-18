#include "BatteryStatus/BatteryProtocol.h"
#include <cassert>
#include <iostream>
int main()
{
    unsigned char corsair[]={1,2,0,0xF8,2}; // 760 tenths
    assert(BatteryProtocol::CorsairLevel(corsair,5)==76);
    assert(!BatteryProtocol::CorsairLevel(corsair,4));
    corsair[0]=2;assert(!BatteryProtocol::CorsairLevel(corsair,5));corsair[0]=1;
    corsair[3]=0xE9;corsair[4]=3;assert(!BatteryProtocol::CorsairLevel(corsair,5));
    unsigned char level[]={0xCC,0x12,7,0,0,5,76};
    assert(BatteryProtocol::AsusLevel(level,7)==76);
    for(size_t n=0;n<7;++n) { assert(!BatteryProtocol::AsusLevel(level,n)); }
    level[6]=101;assert(!BatteryProtocol::AsusLevel(level,7));level[6]=76;
    level[0]=0;assert(!BatteryProtocol::AsusLevel(level,7));level[0]=0xCC;
    level[1]=0xFF;level[2]=0xAA;assert(!BatteryProtocol::AsusLevel(level,7));level[1]=0x12;level[2]=7;
    level[5]=0xFF;level[6]=0xAA;assert(!BatteryProtocol::AsusLevel(level,7));level[5]=5;level[6]=76;
    level[2]=9;assert(!BatteryProtocol::AsusLevel(level,7)); // undocumented event is NOT a reading
    unsigned char charging[]={0xCC,0x12,8,0,0,1,0};
    assert(BatteryProtocol::AsusCharging(charging,7)==1);
    charging[5]=0;assert(BatteryProtocol::AsusCharging(charging,7)==0);
    charging[5]=7;assert(!BatteryProtocol::AsusCharging(charging,7));
    assert(!BatteryProtocol::AsusCharging(charging,5));
    std::cout<<"Protocol validation: lengths, routes, ranges, errors, unknown flags and unrelated events passed\n";
}
