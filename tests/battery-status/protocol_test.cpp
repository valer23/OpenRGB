#include "BatteryStatus/BatteryProtocol.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
int main()
{
    // Real owner-confirmed awake capture. Prefixes are sufficient for decoding;
    // this fixture does not claim a sleeping-device or independent-gauge check.
    std::ifstream capture("tests/battery-status/fixtures/delta-ii-awake-20261019.json");
    assert(capture.good());
    nlohmann::json fixture;
    capture >> fixture;
    const auto& snapshot = fixture.at("snapshot");
    const auto capturedLevel = snapshot.at("level_report_prefix").get<std::vector<unsigned char>>();
    const auto capturedCharging = snapshot.at("charging_report_prefix").get<std::vector<unsigned char>>();
    assert(BatteryProtocol::AsusLevel(capturedLevel.data(), capturedLevel.size()) == 89);
    assert(BatteryProtocol::AsusCharging(capturedCharging.data(), capturedCharging.size()) == 0);
    assert(!BatteryProtocol::AsusLevel(capturedCharging.data(), capturedCharging.size()));
    assert(!BatteryProtocol::AsusCharging(capturedLevel.data(), capturedLevel.size()));
    assert(!snapshot.at("automatic_polling_enabled").get<bool>());
    assert(!snapshot.at("no_wake_validated").get<bool>());
    unsigned char corsair[]={1,2,0,0xF8,2}; // 760 tenths
    assert(BatteryProtocol::CorsairLevel(corsair,5)==76);
    assert(!BatteryProtocol::CorsairLevel(corsair,4));
    corsair[0]=2;assert(!BatteryProtocol::CorsairLevel(corsair,5));corsair[0]=1;
    corsair[3]=0xE9;corsair[4]=3;assert(!BatteryProtocol::CorsairLevel(corsair,5));
    unsigned char notification[]={1,1,0x0F,0,0xF8,2};
    assert(BatteryProtocol::CorsairNotification(notification,6)==76);
    assert(!BatteryProtocol::CorsairNotification(notification,5));
    assert(!BatteryProtocol::CorsairNotification(corsair,5)); // GET replies cannot enter this path
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
