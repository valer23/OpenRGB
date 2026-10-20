#include "BatteryStatus/BatteryStatusManager.h"
// Reused installed 1.0.20261020.1 differential fixture; packet fingerprint from source146effae.
// Private test: actual controller source, fake HID/settings and deterministic clock.
#include "CorsairPeripheralV2Controller.h"
#include "SettingsManager.h"
#include "MouseNativeClock.h"
#include <hidapi_winapi.h>
#include <deque>
#include <iostream>
#include <cstdlib>
#include <fstream>
#define CHECK(x) do {if(!(x)){std::cerr<<"Failed line "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(0)
const corsair_v2_device mouse_device={0x1B7E,DEVICE_TYPE_MOUSE,1,12,{nullptr},nullptr};
const corsair_v2_device keyboard_device={0x9999,DEVICE_TYPE_KEYBOARD,1,12,{nullptr},nullptr};
const corsair_v2_device* entries[]={&mouse_device,&keyboard_device};
const corsair_v2_device** corsair_v2_device_list=entries;
const unsigned int CORSAIR_V2_DEVICE_COUNT=2;
std::vector<std::vector<unsigned char>> writes;
std::vector<unsigned char> last;
std::deque<std::vector<unsigned char>> events;
bool pending=false,receiver=true,missing=false,ambiguous=false,identity_bad=false;
bool fail_hw=false,short_write=false,fail_sw=false,fail_init=false,fail_counter=false,fail_events=false,fail_mask=false;
unsigned int pid=0x1B7E,idle=0,mask=2,counter_length=64,counter_error=0,counter_child=1;
hid_device* control=(hid_device*)1;
hid_device* event_handle=(hid_device*)2;
int event_closes=0;
extern "C" int hid_write(hid_device* dev,const unsigned char* data,size_t size)
{
 CHECK(dev==control);last.assign(data,data+size);writes.push_back(last);pending=true;return short_write ? (int)size-1 : (int)size;
}
extern "C" int hid_read_timeout(hid_device* dev,unsigned char* data,size_t size,int)
{
 if(dev==event_handle) {
  if(fail_events) return -1;
  if(events.empty()) return 0;
  auto report=events.front();events.pop_front();std::memcpy(data,report.data(),report.size());return(int)report.size();
 }
 CHECK(dev==control);
 if(!pending) return 0;
 pending=false;std::memset(data,0,size);
 if(last[2]==2 && last[3]==0x12) {unsigned int product=receiver && last[1]==8 ? 0x1BA6 : pid;data[3]=product&255;data[4]=product>>8;}
 if(last[2]==2 && last[3]==0x36) {if(fail_mask) return 0;data[0]=0;data[1]=2;data[3]=mask;}
 if(last[2]==2 && last[3]==0x40) {
  if(fail_counter) return 0;
  data[0]=counter_child;data[1]=2;data[2]=counter_error;data[3]=idle&255;data[4]=idle>>8;return counter_length;
 }
 if(last[2]==1 && last[3]==3 && last[5]==1 && fail_hw) return 0;
 if(last[2]==1 && last[3]==3 && last[5]==2 && fail_sw) data[2]=3;
 if(last[2]==2 && last[3]==0x5F && fail_init) data[2]=3;
 return 64;
}
extern "C" hid_device_info* hid_enumerate(unsigned short,unsigned short)
{
 static hid_device_info a={},b={},c={};
 a={};b={};c={};a.interface_number=b.interface_number=c.interface_number=2;
 a.usage_page=b.usage_page=c.usage_page=0xFF42;a.usage=b.usage=c.usage=2;
 a.path=(char*)"other";a.serial_number=(wchar_t*)L"other";
 b.path=(char*)"target";b.serial_number=(wchar_t*)L"target";
 c.path=(char*)"duplicate";c.serial_number=(wchar_t*)L"target";
 if(!missing) {a.next=&b;if(ambiguous) b.next=&c;}
 return &a;
}
extern "C" void hid_free_enumeration(hid_device_info*) {}
extern "C" hid_device* hid_open_path(const char* path) {return !std::strcmp(path,"other")?(hid_device*)3:event_handle;}
extern "C" int hid_winapi_get_container_id(hid_device* dev,GUID* id) {if(identity_bad)return -1;*id={};id->Data1=dev==(hid_device*)3?2:1;return 0;}
extern "C" int hid_get_serial_number_string(hid_device*,wchar_t* text,size_t) {if(identity_bad)return -1;std::wcscpy(text,L"target");return 0;}
extern "C" void hid_close(hid_device* dev) {if(dev==event_handle)++event_closes;}
struct Device:CorsairPeripheralV2Controller {
 Device():CorsairPeripheralV2Controller(control,"control","test") {
 auto text=GetMouseIdleConfiguration();
 if(!text.empty()) {
  auto c=nlohmann::json::parse(text);
  c["configuration"]["idle_handoff_enabled"]=mouse_test_settings["enabled"];
  c["configuration"]["idle_timeout_seconds"]=mouse_test_settings["timeout_seconds"];
  SetMouseIdleConfiguration(c.dump());
 }
}
 void SetLedsDirect(std::vector<RGBColor*>) override {}
 void Frame() {unsigned char colours[3]={1,2,3};SetLEDs(colours,3);}
};
unsigned int Mode(int mode) {unsigned int n=0;for(auto& w:writes)if(w[2]==1 && w[3]==3 && w[5]==mode)++n;return n;}
unsigned int Counter() {unsigned int n=0;for(auto& w:writes)if(w[1]==9 && w[2]==2 && w[3]==0x40)++n;return n;}
unsigned int Colours() {unsigned int n=0;for(auto& w:writes)if(w[2]==6)++n;return n;}
void Event(unsigned int bits) {events.push_back({0,1,0x36,0,(unsigned char)bits});mask=bits;}

unsigned int random_state=923521;
unsigned int Random() { random_state=random_state*1664525U+1013904223U;return random_state; }
int main() {
 auto& cache=BatteryStatusManager::Get(); const auto token=cache.Acquire(); cache.SetCorsairValidated(true);
 unsigned long long hash=1469598103934665603ULL;
 unsigned int operations=0;
 {
  mouse_test_now=1000;idle=0;mask=2;Device d;writes.clear();
  for(unsigned int step=0;step<20000;++step) {
   mouse_test_now+=Random()%1300;
   idle=(Random()%17==0)?0:idle+1;
   fail_sw=Random()%11==0;fail_init=Random()%13==0;fail_hw=Random()%17==0;
   fail_counter=Random()%19==0;fail_events=Random()%23==0;fail_mask=Random()%29==0;
   short_write=Random()%31==0;missing=Random()%37==0;
   if(Random()%13==0)Event((Random()>>8)&6);
   if(Random()%41==0) {
    auto c=nlohmann::json::parse(d.GetMouseIdleConfiguration());
    c["configuration"]["idle_handoff_enabled"]=(Random()%4!=0);
    c["configuration"]["idle_timeout_seconds"]=5+Random()%60;
    d.SetMouseIdleConfiguration(c.dump());
   }
   bool animated_frame=Random()%3==0;
   bool needs_update=d.MouseIdleNeedsUpdate();
   if(needs_update || animated_frame)d.Frame();
   hash^=step;hash*=1099511628211ULL;
   for(const auto& w:writes) {
    ++operations;
    for(unsigned char b:w) { hash^=b;hash*=1099511628211ULL; }
    if(w[2]==1 && w[3]==3)idle=0;
   }
   writes.clear();
  }
 }
 cache.Release(token);
 CHECK(operations==23529 && hash==4169482241693242891ULL);
 std::cout<<"20000 deterministic mixed animation/static/failure/configuration steps: "<<operations<<" packets, trace hash "<<hash<<"\n";
}
