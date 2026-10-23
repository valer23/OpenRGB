/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "AsusBatteryLog.h"
#include "WindowsHidTransfer.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <vector>
#ifdef _WIN32
#include <wincrypt.h>
#endif

namespace
{
using json = nlohmann::json;
json Missing(const std::string& detail)
{
    return {{"source","asus_log"},{"percentage",nullptr},{"charging",nullptr},
        {"availability","unavailable"},{"error",detail}};
}
std::optional<uint64_t> Timestamp(const std::string& line)
{
    if(line.size()<25 || line[0]!='[' || line[20]!='.' || line[24]!=']') { return {}; }
    for(size_t i=21;i<24;++i) { if(line[i]<'0' || line[i]>'9') { return {}; } }
    std::tm value={};
    std::istringstream in(line.substr(1,19));
    in>>std::get_time(&value,"%Y/%m/%d %H:%M:%S");
    if(in.fail() || value.tm_year<100 || value.tm_year>200) { return {}; }
    const auto expected=value;
    value.tm_isdst=-1;
    auto epoch=std::mktime(&value);
    if(epoch<0 || value.tm_year!=expected.tm_year || value.tm_mon!=expected.tm_mon ||
       value.tm_mday!=expected.tm_mday || value.tm_hour!=expected.tm_hour ||
       value.tm_min!=expected.tm_min || value.tm_sec!=expected.tm_sec) { return {}; }
    // At a DST overlap, prefer the older matching local timestamp, so ambiguity
    // cannot make a retained report look newer than it may actually be.
    for(int dst:{0,1})
    {
        auto candidate=expected;candidate.tm_isdst=dst;
        const auto alternate=std::mktime(&candidate);
        if(alternate>=0 && candidate.tm_year==expected.tm_year && candidate.tm_mon==expected.tm_mon &&
           candidate.tm_mday==expected.tm_mday && candidate.tm_hour==expected.tm_hour &&
           candidate.tm_min==expected.tm_min && candidate.tm_sec==expected.tm_sec)
        { epoch=std::min(epoch,alternate); }
    }
    return static_cast<uint64_t>(epoch)*1000+std::stoul(line.substr(21,3));
}
#ifdef _WIN32
WindowsHidTransfer& FileTransfer()
{
    // Separate from the HID transport; this instance receives regular log files
    // only, opened GENERIC_READ with no device commands or write operations.
    static auto* transfer=new WindowsHidTransfer(WindowsHidTransfer::Api{});
    return *transfer;
}
std::filesystem::path LogDirectory()
{
    std::wstring root;
    for(REGSAM view:{KEY_WOW64_64KEY,KEY_WOW64_32KEY})
    {
        HKEY key=nullptr;
        if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\ASUS\\ARMOURY CRATE Diagnosis",0,KEY_READ|view,&key)!=ERROR_SUCCESS) { continue; }
        wchar_t value[4096]={};DWORD size=sizeof(value),type=0;
        const auto status=RegQueryValueExW(key,L"LogPath",nullptr,&type,reinterpret_cast<BYTE*>(value),&size);
        RegCloseKey(key);
        if(status==ERROR_SUCCESS && type==REG_SZ && size>=sizeof(wchar_t) && size<sizeof(value)) { root=value;break; }
    }
    // Only a normal local drive path is accepted, never a network/device path.
    if(root.size()<3 || !((root[0]>=L'A'&&root[0]<=L'Z')||(root[0]>=L'a'&&root[0]<=L'z')) ||
       root[1]!=L':' || (root[2]!=L'\\' && root[2]!=L'/')) { return {}; }
    wchar_t user[256]={};DWORD count=256;
    if(!GetUserNameW(user,&count)) { return {}; }
    const int length=WideCharToMultiByte(CP_UTF8,0,user,-1,nullptr,0,nullptr,nullptr);
    if(length<=1) { return {}; }
    std::string utf8(static_cast<size_t>(length),'\0');
    WideCharToMultiByte(CP_UTF8,0,user,-1,utf8.data(),length,nullptr,nullptr);utf8.pop_back();
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;
    if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT)) { return {}; }
    BYTE bytes[16]={};DWORD size=sizeof(bytes);
    const bool ok=CryptCreateHash(provider,CALG_MD5,0,0,&hash) &&
        CryptHashData(hash,reinterpret_cast<const BYTE*>(utf8.data()),static_cast<DWORD>(utf8.size()),0) &&
        CryptGetHashParam(hash,HP_HASHVAL,bytes,&size,0);
    if(hash) { CryptDestroyHash(hash); }CryptReleaseContext(provider,0);
    if(!ok || size!=16) { return {}; }
    std::ostringstream group;
    for(BYTE byte:bytes) { group<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned int>(byte); }
    return std::filesystem::path(root)/L"Framework"/group.str();
}
#endif
bool Tail(const std::filesystem::path& path,std::string& text)
{
#ifdef _WIN32
    auto& io=FileTransfer();
    if(!io.Ready()) { return false; }
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(file==INVALID_HANDLE_VALUE) { return false; }
    LARGE_INTEGER size={};BY_HANDLE_FILE_INFORMATION info={};
    bool ok=GetFileType(file)==FILE_TYPE_DISK && GetFileInformationByHandle(file,&info) &&
        !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && GetFileSizeEx(file,&size) && size.QuadPart>=0;
    if(ok)
    {
        const auto amount=static_cast<DWORD>(std::min<LONGLONG>(65536,size.QuadPart));
        text.resize(amount);
        const int count=amount ? io.ReadAt(file,reinterpret_cast<unsigned char*>(text.data()),amount,250,
            static_cast<uint64_t>(size.QuadPart)-amount) : 0;
        ok=count>=0 && static_cast<DWORD>(count)==amount;
        if(ok) { text.resize(static_cast<size_t>(count)); }
    }
    if(file!=INVALID_HANDLE_VALUE) { CloseHandle(file); }
    return ok;
#else
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file) { return false; }
    auto size=file.tellg();if(size<0) { return false; }
    const auto amount=std::min<std::streamoff>(65536,size);
    file.seekg(size-amount);text.resize(static_cast<size_t>(amount));
    return static_cast<bool>(file.read(text.data(),amount));
#endif
}
}

nlohmann::json AsusBatteryLog::Parse(const std::string& text,uint64_t now_ms)
{
    json latest=nullptr,sample=nullptr;
    uint64_t latest_time=0,sample_time=0;
    bool latest_conflict=false,sample_conflict=false;
    size_t offset=0,records=0;
    while(offset<text.size() && records++<2048)
    {
        const auto next=text.find("\n[",offset+1);
        const auto end=next==std::string::npos ? text.size() : next;
        const auto line=text.substr(offset,std::min<size_t>(end-offset,8193));
        offset=next==std::string::npos ? text.size() : next+1;
        const auto marker=line.find("[I] [Device Service Broadcast] ");
        if(marker!=25 || line.size()>8192) { continue; }
        const auto stamp=Timestamp(line);
        if(!stamp || *stamp>now_ms) { continue; }
        const auto brace=line.find('{',marker);
        if(brace==std::string::npos) { continue; }
        const auto data=json::parse(line.substr(brace),nullptr,false);
        if(!data.is_object() || data.value("command",json())!="broadcastEvent" ||
           !data.contains("target") || !data["target"].is_object() || data["target"].value("role",json())!="trayIcon" ||
           !data.contains("msg") || !data["msg"].is_object()) { continue; }
        const auto& msg=data["msg"];
        if(msg.value("cmd",json())!="10200" || msg.value("modelNumber",json())!="6906" ||
           !msg.contains("powerStatus") || !msg["powerStatus"].is_number_integer() ||
           msg["powerStatus"] < -2 || msg["powerStatus"] > 100) { continue; }
        // Compare only fields consumed by this provider. ASUS may broadcast the
        // same measurement twice, adding unrelated metadata such as isHPC.
        const auto boolean=[&msg](const char* key)->json {
            const auto found=msg.find(key);
            return found!=msg.end() && found->is_boolean() ? *found : json(nullptr);
        };
        const json report={{"powerStatus",msg["powerStatus"]},
            {"isCharging",boolean("isCharging")},{"isDeviceAlive",boolean("isDeviceAlive")}};
        if(latest.is_null() || *stamp>latest_time)
        { latest=report;latest_time=*stamp;latest_conflict=false; }
        else if(*stamp==latest_time && latest!=report) { latest_conflict=true; }
        if(msg["powerStatus"]>=0)
        {
            if(sample.is_null() || *stamp>sample_time)
            { sample=report;sample_time=*stamp;sample_conflict=false; }
            else if(*stamp==sample_time && sample!=report) { sample_conflict=true; }
        }
    }
    if(latest_conflict || sample_conflict) { return Missing("Conflicting ASUS cache reports at one timestamp"); }
    if(sample.is_null()) { return Missing("No complete Delta II battery report in the existing ASUS logs"); }
    const auto charge=sample.find("isCharging");
    const bool offline=latest.contains("isDeviceAlive") && latest["isDeviceAlive"]==false;
    return {{"source","asus_log"},{"percentage",sample["powerStatus"]},
        {"charging",charge!=sample.end()&&charge->is_boolean()?*charge:json(nullptr)},
        {"source_timestamp_ms",sample_time},{"source_age_ms",now_ms-sample_time},
        {"availability",offline?"asus_cache_offline":"asus_cache"},
        {"detail","ASUS cached report; measurement time and headset sleep state are unknown"}};
}

nlohmann::json AsusBatteryLog::ReadDirectory(const std::filesystem::path& directory,uint64_t now_ms)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> guard(mutex);
    std::error_code error;
    if(directory.empty() || std::filesystem::is_symlink(directory,error)) { return Missing("ASUS log directory unavailable"); }
    // ASUS retains six previous local calendar days and six rotated backups.
    // Exact names bound discovery independently of unrelated directory entries.
    const std::time_t seconds=static_cast<std::time_t>(now_ms/1000);
    std::tm day={};
#ifdef _WIN32
    if(localtime_s(&day,&seconds)!=0) { return Missing("ASUS cache date unavailable"); }
#else
    if(!localtime_r(&seconds,&day)) { return Missing("ASUS cache date unavailable"); }
#endif
    day.tm_hour=12;day.tm_min=0;day.tm_sec=0;
    std::string text;
    size_t files=0;
    for(int age=0;age<=6 && files<16;++age)
    {
        std::ostringstream date;date<<std::put_time(&day,"%Y-%m-%d");
        for(int rotation=0;rotation<=6 && files<16;++rotation)
        {
            const auto name="Service.04."+date.str()+
                (rotation ? "."+std::to_string(rotation) : "")+".log";
            const auto path=directory/name;
            const auto status=std::filesystem::symlink_status(path,error);
            if(error || !std::filesystem::is_regular_file(status)) { error.clear();continue; }
            ++files;
            std::string part;
            if(Tail(path,part)) { text+=part;text+='\n'; }
        }
        --day.tm_mday;day.tm_isdst=-1;
        if(std::mktime(&day)<0) { break; }
    }
    return Parse(text,now_ms);
}

nlohmann::json AsusBatteryLog::Read()
{
#ifdef _WIN32
    const auto directory=LogDirectory();
    if(directory.empty()) { return Missing("Existing ASUS diagnostic log location is unavailable; no settings changed"); }
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return ReadDirectory(directory,static_cast<uint64_t>(std::max<int64_t>(0,now)));
#else
    return Missing("Automatic ASUS cached-log discovery is currently Windows-only");
#endif
}
