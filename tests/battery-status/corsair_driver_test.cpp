#include "BatteryStatus/BatteryStatusManager.h"
// Private test: actual controller source, fake HID/settings and deterministic clock.
#include "CorsairPeripheralV2Controller.h"
#include "SettingsManager.h"
#include "MouseNativeClock.h"
#include <hidapi_winapi.h>
#include <deque>
#include <iostream>
#include <cstdlib>
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
bool fail_sw=false,fail_init=false,fail_counter=false,fail_events=false,fail_mask=false;
unsigned int battery_level=760,battery_length=64,battery_child=1,battery_status=0;
unsigned int pid=0x1B7E,idle=0,mask=2,counter_length=64,counter_error=0,counter_child=1;
hid_device* control=(hid_device*)1;
hid_device* event_handle=(hid_device*)2;
int event_closes=0;
extern "C" int hid_write(hid_device* dev,const unsigned char* data,size_t size)
{
 CHECK(dev==control);last.assign(data,data+size);writes.push_back(last);pending=true;return(int)size;
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
 if(last[2]==2 && last[3]==0x0F) {
  data[0]=battery_child;data[1]=2;data[2]=battery_status;data[3]=battery_level&255;data[4]=battery_level>>8;return battery_length;
 }
 if(last[2]==2 && last[3]==0x40) {
  if(fail_counter) return 0;
  data[0]=counter_child;data[1]=2;data[2]=counter_error;data[3]=idle&255;data[4]=idle>>8;return counter_length;
 }
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
 Device():CorsairPeripheralV2Controller(control,"control","test"){}
 void SetLedsDirect(std::vector<RGBColor*>) override {}
 void Frame() {unsigned char colours[3]={1,2,3};SetLEDs(colours,3);}
};
unsigned int Mode(int mode) {unsigned int n=0;for(auto& w:writes)if(w[2]==1 && w[3]==3 && w[5]==mode)++n;return n;}
unsigned int Counter() {unsigned int n=0;for(auto& w:writes)if(w[1]==9 && w[2]==2 && w[3]==0x40)++n;return n;}
unsigned int Colours() {unsigned int n=0;for(auto& w:writes)if(w[2]==6)++n;return n;}
unsigned int BatteryGets() {unsigned int n=0;for(auto& w:writes)if(w[2]==2 && w[3]==0x0F)++n;return n;}
void Event(unsigned int bits) {events.push_back({0,1,0x36,0,(unsigned char)bits});mask=bits;}
int main()
{
 { // Hardware regression: changing rendering ownership resets GET40 to zero.
  mouse_test_now=1000;idle=0;mask=2;
  Device d;CHECK(d.HasMouseIdleControl());writes.clear();d.Frame();writes.clear();
  mouse_test_now=31000;idle=30;d.Frame();CHECK(Mode(1)==1 && Colours()==0);writes.clear();
  for(unsigned int second=1;second<=6;++second) {
   mouse_test_now=31000+second*1000;idle=second<=2 ? 0 : second-1;
   d.Frame();CHECK(Mode(2)==0 && Colours()==0);writes.clear();
  }
  mouse_test_now=38000;idle=0;d.Frame();CHECK(Mode(2)==1 && Colours()==1);writes.clear();
 }
 { // No input for three minutes after handoff must never resume effects.
  writes.clear();mouse_test_now=1000;idle=0;
  Device d;writes.clear();mouse_test_now=31000;idle=30;d.Frame();writes.clear();
  for(unsigned int second=1;second<=180;++second) {
   mouse_test_now=31000+second*1000;idle=second-1;
   d.Frame();CHECK(Mode(2)==0 && Colours()==0);writes.clear();
  }
  mouse_test_now+=1000;idle=0;d.Frame();CHECK(Mode(2)==1 && Colours()==1);writes.clear();
 }
 { // Continued input after the reset must eventually be distinguishable.
  writes.clear();mouse_test_now=1000;idle=0;
  Device d;writes.clear();mouse_test_now=31000;idle=30;d.Frame();writes.clear();
  for(unsigned int second=1;second<=2;++second) {
   mouse_test_now=31000+second*1000;idle=0;d.Frame();CHECK(Mode(2)==0);writes.clear();
  }
  mouse_test_now=34000;idle=0;d.Frame();CHECK(Mode(2)==1 && Colours()==1);writes.clear();
 }
 writes.clear();mouse_test_now=0;idle=0;
 {
  Device d;CHECK(d.HasMouseIdleControl());writes.clear();
  d.Frame();CHECK(Counter()==1 && Colours()==1);writes.clear();
  for(int i=0;i<100;i++) { d.Frame(); }CHECK(Counter()==0 && Colours()==100);writes.clear();
  idle=30;mouse_test_now=31000;CHECK(d.MouseIdleNeedsUpdate());d.Frame();CHECK(Counter()==1 && Mode(1)==1 && Colours()==0);writes.clear();
  for(int i=0;i<5000;i++) { d.Frame(); }CHECK(writes.empty());
  mouse_test_now=32000;CHECK(d.MouseIdleNeedsUpdate());d.Frame();CHECK(Counter()==1 && Mode(1)==0 && Colours()==0);writes.clear();
  fail_counter=true;mouse_test_now=33000;d.Frame();CHECK(Counter()==1 && Mode(2)==0 && Colours()==0);writes.clear();fail_counter=false;
  counter_error=5;mouse_test_now=34000;d.Frame();CHECK(Mode(2)==0 && Colours()==0);writes.clear();counter_error=0;
  counter_length=3;mouse_test_now=35000;d.Frame();CHECK(Mode(2)==0 && Colours()==0);writes.clear();counter_length=64;
  counter_child=2;mouse_test_now=36000;d.Frame();CHECK(Mode(2)==0 && Colours()==0);writes.clear();counter_child=1;
  idle=0;fail_init=true;mouse_test_now=37000;d.Frame();CHECK(Mode(2)==1 && Colours()==0);writes.clear();
  for(int i=0;i<1000;i++) { d.Frame(); }CHECK(writes.empty());
  fail_init=false;mouse_test_now=38000;d.Frame();CHECK(Mode(2)==1 && Colours()==1);writes.clear();
  Event(0);CHECK(!d.MouseIdleNeedsUpdate());for(int i=0;i<5000;i++) { d.Frame(); }CHECK(writes.empty());
  mouse_test_now=1000000;Event(4);CHECK(!d.MouseIdleNeedsUpdate());d.Frame();CHECK(writes.empty());
  Event(6);idle=0;CHECK(d.MouseIdleNeedsUpdate());d.Frame();CHECK(Mode(2)==1 && Colours()==1);writes.clear();
  Event(2);d.Frame();CHECK(Mode(2)==0 && Counter()==0 && Colours()==1);writes.clear();
  events.push_back({0,1,0x36});d.Frame();CHECK(Mode(2)==0 && Colours()==1);writes.clear();
  fail_events=true;CHECK(!d.MouseIdleNeedsUpdate());d.Frame();CHECK(writes.empty());
  fail_events=false;mouse_test_now+=1000;mask=2;CHECK(d.MouseIdleNeedsUpdate());d.Frame();CHECK(Mode(2)==1 && Colours()==1);writes.clear();
  fail_events=true;CHECK(!d.MouseIdleNeedsUpdate());d.Frame();CHECK(writes.empty());
  fail_events=false;mouse_test_now+=1000;idle=30;CHECK(d.MouseIdleNeedsUpdate());d.Frame();CHECK(Mode(1)==1 && Mode(2)==0 && Colours()==0);writes.clear();
  mouse_test_now+=1000;d.Frame();CHECK(Mode(1)==0 && Colours()==0);writes.clear();
  mouse_test_now+=1000;idle=0;d.Frame();CHECK(Mode(2)==1 && Colours()==1);writes.clear();
  fail_events=true;CHECK(!d.MouseIdleNeedsUpdate());d.Frame();CHECK(writes.empty());
  fail_events=false;mouse_test_now+=1000;mask=0;CHECK(d.MouseIdleNeedsUpdate());d.Frame();CHECK(writes.size()==1 && writes[0][1]==8 && Counter()==0);writes.clear();
  mouse_test_now+=100000;d.Frame();CHECK(writes.empty());
 }
 CHECK(writes.empty()); // confirmed asleep destructor must not address mouse
 receiver=false;{Device d;CHECK(!d.HasMouseIdleControl());}receiver=true;
 pid=0x9999;{Device d;CHECK(!d.HasMouseIdleControl());}pid=0x1B7E;
 mouse_test_settings["enabled"]=false;{Device d;CHECK(!d.HasMouseIdleControl());}mouse_test_settings["enabled"]=true;
 missing=true;{Device d;CHECK(!d.HasMouseIdleControl());}missing=false;
 ambiguous=true;{Device d;CHECK(!d.HasMouseIdleControl());}ambiguous=false;
 identity_bad=true;{Device d;CHECK(!d.HasMouseIdleControl());}identity_bad=false;
 mask=2;fail_mask=true;{Device d;CHECK(!d.HasMouseIdleControl());}fail_mask=false;
 fail_counter=true;{Device d;CHECK(!d.HasMouseIdleControl());}fail_counter=false;
 { // Only keyed passive notifications may publish battery values.
  auto& cache=BatteryStatusManager::Get();
  mouse_test_now=1000;idle=0;mask=2;writes.clear();Device d;writes.clear();
  d.Frame();CHECK(BatteryGets()==0);
  auto token=cache.Acquire();d.Frame();CHECK(BatteryGets()==0);
  events.push_back({1,1,0x0F,0,0xF8,2});d.Frame();CHECK(cache.Snapshots()[0]["percentage"].is_null());
  cache.SetCorsairValidated(true);writes.clear();
  events.push_back({1,1,0x0F,0,0xF8,2});d.Frame();CHECK(BatteryGets()==0);
  CHECK(cache.Snapshots()[0]["percentage"]==76);writes.clear();
  events.push_back({1,2,0,0,0,0});d.Frame();CHECK(cache.Snapshots()[0]["percentage"]==76);
  events.push_back({2,1,0x0F,0,0x20,3});d.Frame();CHECK(cache.Snapshots()[0]["percentage"]==76);
  events.push_back({1,1,0x0F,0,0xFF,0xFF});d.Frame();CHECK(cache.Snapshots()[0]["percentage"]==76);
  events.push_back({1,1,0x0F});d.Frame();CHECK(cache.Snapshots()[0]["percentage"]==76);
  mouse_test_now=31000;idle=30;d.Frame();CHECK(BatteryGets()==0 && Mode(1)==1);writes.clear();
  events.push_back({1,1,0x0F,0,0xEE,2});d.Frame();CHECK(BatteryGets()==0 && writes.empty());
  CHECK(cache.Snapshots()[0]["percentage"]==75 && cache.Snapshots()[0]["availability"]=="idle");
  Event(0);d.Frame();CHECK(BatteryGets()==0 && writes.empty());
  events.push_back({1,1,0x0F,0,0x20,3});d.Frame();CHECK(cache.Snapshots()[0]["percentage"]==75);
  CHECK(cache.Snapshots()[0]["availability"]=="disconnected");
  cache.Release(token);cache.SetCorsairValidated(false);
 }
 std::cout<<"Native actual-driver checks passed: counter cadence/decoding, silent idle colours, offline zero writes, child mask isolation, checked wake recovery, listener recovery and startup/non-target guards.\n";
}
