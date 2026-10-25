// Actual Windows observer, fake detection consumer: NO HID reader/command code linked.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "DetectionManager.h"
#include "Controllers/CorsairPeripheralV2Controller/CorsairStartupRecovery_Windows.h"
HWND RegisteredMouse()
{
 UINT count=0;GetRegisteredRawInputDevices(nullptr,&count,sizeof(RAWINPUTDEVICE));
 std::vector<RAWINPUTDEVICE> values(count);
 if(count)GetRegisteredRawInputDevices(values.data(),&count,sizeof(RAWINPUTDEVICE));
 for(const auto& value:values)if(value.usUsagePage==1 && value.usUsage==2)return value.hwndTarget;
 return nullptr;
}
int main(int argc,char**argv)
{
 if(argc==2 && std::string(argv[1])=="--list-mice")
 {
  UINT count=0;GetRawInputDeviceList(nullptr,&count,sizeof(RAWINPUTDEVICELIST));
  std::vector<RAWINPUTDEVICELIST> devices(count);
  if(count)GetRawInputDeviceList(devices.data(),&count,sizeof(RAWINPUTDEVICELIST));
  for(const auto& device:devices)if(device.dwType==RIM_TYPEMOUSE)
  {
   UINT length=0;GetRawInputDeviceInfoW(device.hDevice,RIDI_DEVICENAME,nullptr,&length);
   if(!length || length>4096)continue;
   std::vector<wchar_t> name(length+1,0);
   if(GetRawInputDeviceInfoW(device.hDevice,RIDI_DEVICENAME,name.data(),&length)!=static_cast<UINT>(-1))std::wcout<<name.data()<<L'\n';
  }
  return 0;
 }
 if(argc!=3 || (std::string(argv[1])!="--shadow" && std::string(argv[1])!="--registrations"))return 2;
 if(std::string(argv[1])=="--shadow")
 {
  StartCorsairStartupRecovery();WatchMissingCorsairReceiver(argv[2]);
  std::cout<<"Observer ready: normal mouse input only; no HID handle, USB query, coordinates or buttons logged"<<std::endl;
  for(int i=0;i<1200 && !recovery_shadow_events.load();++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));
  StopCorsairStartupRecovery();
  std::cout<<"matched_receiver_activity="<<recovery_shadow_events.load()<<"; device_commands=0"<<std::endl;
  return recovery_shadow_events.load()==1?0:1;
 }
 recovery_shadow_accept=false;
 HWND dummy=CreateWindowExW(0,L"STATIC",L"",0,0,0,0,0,HWND_MESSAGE,nullptr,GetModuleHandleW(nullptr),nullptr);
 assert(dummy);
 for(bool pageWide:{false,true})
 {
  RAWINPUTDEVICE claim{1,static_cast<USHORT>(pageWide?0:2),static_cast<DWORD>(RIDEV_INPUTSINK|(pageWide?RIDEV_PAGEONLY:0)),dummy};
  assert(RegisterRawInputDevices(&claim,1,sizeof(claim)));
  StartCorsairStartupRecovery();WatchMissingCorsairReceiver(argv[2]);StopCorsairStartupRecovery();
  UINT count=0;GetRegisteredRawInputDevices(nullptr,&count,sizeof(RAWINPUTDEVICE));
  std::vector<RAWINPUTDEVICE> registered(count);GetRegisteredRawInputDevices(registered.data(),&count,sizeof(RAWINPUTDEVICE));
  bool preserved=false;for(const auto& value:registered)preserved|=value.usUsagePage==1 && value.usUsage==claim.usUsage && value.hwndTarget==dummy;
  assert(preserved);
  claim.dwFlags=RIDEV_REMOVE|(pageWide?RIDEV_PAGEONLY:0);claim.hwndTarget=nullptr;assert(RegisterRawInputDevices(&claim,1,sizeof(claim)));
 }
 StartCorsairStartupRecovery();WatchMissingCorsairReceiver(argv[2]);
 HWND original=RegisteredMouse();assert(original && original!=dummy);
 ForgetMissingCorsairReceiver(argv[2]);
 for(int i=0;i<100 && RegisteredMouse();++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
 assert(!RegisteredMouse());
 WatchMissingCorsairReceiver(argv[2]);
 for(int i=0;i<100 && !RegisteredMouse();++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
 assert(RegisteredMouse()==original);
 RAWINPUTDEVICE takeover{1,2,RIDEV_INPUTSINK,dummy};assert(RegisterRawInputDevices(&takeover,1,sizeof(takeover)));
 StopCorsairStartupRecovery();assert(RegisteredMouse()==dummy);
 takeover.dwFlags=RIDEV_REMOVE;takeover.hwndTarget=nullptr;assert(RegisterRawInputDevices(&takeover,1,sizeof(takeover)));
 DestroyWindow(dummy);
 std::cout<<"PASS: mouse/page registration conflicts preserved; replacement registration survives observer shutdown; idle unsubscribe/rearm; no HID commands\n";
}
