#include "BatteryStatus/AsusBatteryLog.h"
#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
using nlohmann::json;
std::string Event(const std::string& time,int percentage=83,bool alive=true) {
    return "[2026/10/21 "+time+"][I] [Device Service Broadcast] "+json{
        {"command","broadcastEvent"},{"target",{{"role","trayIcon"}}},
        {"msg",{{"cmd","10200"},{"modelNumber","6906"},{"modelName","ROG DELTA II"},
        {"powerStatus",percentage},{"isCharging",false},{"isDeviceAlive",alive},{"levels",5}}}}.dump(4)+"\n";
}
int main() {
    std::tm local={};local.tm_year=126;local.tm_mon=9;local.tm_mday=21;local.tm_hour=1;local.tm_isdst=-1;
    const uint64_t now=static_cast<uint64_t>(std::mktime(&local))*1000;
    auto one=Event("00:34:22.258");
    auto value=AsusBatteryLog::Parse(one,now);
    assert(value["percentage"]==83 && value["charging"]==false);
    assert(value["source_age_ms"]==1537742 && value["source"]=="asus_log");
    assert(value["availability"]=="asus_cache");
    auto off=Event("00:45:35.096",83,false);
    value=AsusBatteryLog::Parse(one+off,now);
    assert(value["percentage"]==83 && value["availability"]=="asus_cache_offline");
    assert(value["source_age_ms"]==864904);
    value=AsusBatteryLog::Parse(one+off.substr(0,off.size()-5),now);
    assert(value["availability"]=="asus_cache" && value["source_age_ms"]==1537742);
    auto wrong=Event("00:50:00.000",99);wrong.replace(wrong.find("6906"),4,"6907");
    assert(AsusBatteryLog::Parse(one+wrong,now)["percentage"]==83);
    assert(AsusBatteryLog::Parse(Event("01:10:00.000"),now)["percentage"].is_null());
    assert(AsusBatteryLog::Parse(Event("00:50:00.000",101),now)["percentage"].is_null());
    assert(AsusBatteryLog::Parse(Event("00:50:00.000",-2),now)["percentage"].is_null());
    value=AsusBatteryLog::Parse(one+Event("00:50:00.000",-1,false),now);
    assert(value["percentage"]==83 && value["availability"]=="asus_cache_offline");
    assert(value["source_age_ms"]==1537742);
    auto unknown=one;unknown.replace(unknown.find("false"),5,"null");
    assert(AsusBatteryLog::Parse(unknown,now)["charging"].is_null());
    auto conflict=Event("00:34:22.258",84);
    assert(AsusBatteryLog::Parse(one+conflict,now)["percentage"].is_null());
    auto dir=std::filesystem::temp_directory_path()/"openrgb-battery-log-test";
    std::filesystem::create_directories(dir);
    for(int i=0;i<160;++i) { std::ofstream f(dir/("000-unrelated-"+std::to_string(i)+".log"));f<<"not a battery report"; }
    {std::ofstream f(dir/"Service.04.2026-10-21.1.log");f<<one;}
    {std::ofstream f(dir/"Service.04.2026-10-21.log");f<<std::string(70000,'x')<<"\n"<<off;}
    {std::ofstream f(dir/"Other.04.2026-10-21.log");f<<Event("00:55:00.000",99);}
    assert(AsusBatteryLog::ReadDirectory(dir,now)["percentage"]==83);
#ifdef _WIN32
    const auto path=dir/L"Service.04.2026-10-21.log";
    HANDLE writer=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(writer!=INVALID_HANDLE_VALUE);
    assert(AsusBatteryLog::ReadDirectory(dir,now)["availability"]=="asus_cache_offline");
    CloseHandle(writer);
#endif
    // Remove only the three known fixture files, never recursively delete a computed directory.
    for(const auto* name:{"Service.04.2026-10-21.1.log","Service.04.2026-10-21.log","Other.04.2026-10-21.log"})std::filesystem::remove(dir/name);
    for(int i=0;i<160;++i)std::filesystem::remove(dir/("000-unrelated-"+std::to_string(i)+".log"));
    std::filesystem::remove(dir);
    assert(AsusBatteryLog::ReadDirectory(dir,now)["percentage"].is_null());
    std::cout<<"ASUS log cache: exact model, partial JSON, offline/unknown retention, source age, rotation and shared writer passed\n";
}
