#include "RGBController_GalaxGPUv1.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
unsigned int logged_errors=0;
struct Write {unsigned char reg;std::vector<unsigned char> data;};
struct Bus: i2c_smbus_interface {
 unsigned char regs[256]={0x11,0,255,0,0,0,0x20,1};
 unsigned int failures[256]={},write_failures[256]={},reads[256]={};
 bool short_read=false;
 std::vector<Write> writes;
 Bus(){std::strcpy(info.device_name,"Mock NVIDIA I2C");}
 s32 i2c_xfer(u8,char,int*,u8*) override{return -1;}
 s32 i2c_smbus_xfer(u8 addr,char rw,u8 reg,int mode,i2c_smbus_data*data)override{
  assert(addr==0x23);assert(mode==I2C_SMBUS_BYTE_DATA || mode==I2C_SMBUS_I2C_BLOCK_DATA);
  unsigned int size=mode==I2C_SMBUS_BYTE_DATA?1:data->block[0];assert(reg+size<=256);
  if(rw==I2C_SMBUS_READ){++reads[reg];if(failures[reg]){--failures[reg];return -1;}
   if(mode==I2C_SMBUS_BYTE_DATA)data->byte=regs[reg];else{std::memcpy(data->block+1,regs+reg,size);if(short_read)data->block[0]=size-1;}return 0;}
  if(write_failures[reg]){--write_failures[reg];return -1;}
  unsigned char*bytes=mode==I2C_SMBUS_BYTE_DATA?&data->byte:data->block+1;
  writes.push_back({reg,{bytes,bytes+size}});std::memcpy(regs+reg,bytes,size);return 0;
 }
};
void write_is(const Write&w,unsigned char reg,std::vector<unsigned char>bytes){assert(w.reg==reg && w.data==bytes);}
int main(){
 // Older devices retain their byte-wise colours and all four mode commands.
 Bus old;GalaxGPUv1Controller legacy(&old,0x23,"Legacy");assert(!legacy.IsEXGamer());
 legacy.SetLEDColorsDirect(1,2,3);assert(old.writes.size()==3);
 write_is(old.writes[0],2,{1});write_is(old.writes[1],3,{2});write_is(old.writes[2],4,{3});
 old.writes.clear();legacy.SetLEDColorsEffect(4,5,6);assert(old.writes.size()==3);
 write_is(old.writes[0],2,{4});write_is(old.writes[1],3,{5});write_is(old.writes[2],4,{6});
 unsigned char expected[4][2]={{0,1},{4,0},{0x84,2},{0x84,0x40}};
 for(unsigned int mode=1;mode<=4;mode++){old.writes.clear();legacy.SetMode(mode);assert(old.writes.size()==2);write_is(old.writes[0],5,{expected[mode-1][0]});write_is(old.writes[1],6,{expected[mode-1][1]});}
 old.regs[5]=0;old.regs[6]=1;old.writes.clear();
 {RGBController_GalaxGPUv1 presentation(new GalaxGPUv1Controller(&old,0x23,"Legacy"));assert(presentation.modes.size()==4 && presentation.modes[0].name=="Direct");assert(old.writes.empty());}
 // EX detection is read-only, bounded, and rejects truncated or wrong signatures.
 Bus bus;bus.failures[0]=2;assert(GalaxGPUv1Controller::TestForEXGamer(&bus,0x23));assert(bus.reads[0]==3);
 bus.short_read=true;assert(!GalaxGPUv1Controller::TestForEXGamer(&bus,0x23));bus.short_read=false;
 bus.regs[0]=0x27;assert(!GalaxGPUv1Controller::TestForEXGamer(&bus,0x23));bus.regs[0]=0x11;
 RGBController_GalaxGPUv1 gpu(new GalaxGPUv1Controller(&bus,0x23,"KFA2 GeForce RTX 3080 Ti EX Gamer",true));
 assert(gpu.modes.size()==2 && gpu.modes[0].name=="Static" && gpu.modes[1].name=="Motherboard Sync");
 assert(gpu.active_mode==1 && bus.writes.empty());gpu.DeviceUpdateLEDs();assert(bus.writes.empty());
 gpu.active_mode=0;bus.write_failures[7]=1;gpu.DeviceUpdateMode();assert(bus.writes.size()==5);write_is(bus.writes[0],7,{0});
 bus.writes.clear();gpu.colors[0]=ToRGBColor(0,255,0);gpu.DeviceUpdateLEDs();assert(bus.writes.size()==4);
 write_is(bus.writes[0],2,{0,255,0});write_is(bus.writes[1],5,{0});write_is(bus.writes[2],6,{0x20});write_is(bus.writes[3],8,{0x5a});
 bus.writes.clear();gpu.DeviceUpdateZoneLEDs(-1);gpu.DeviceUpdateSingleLED(1);assert(bus.writes.empty());
 // A failure is logged once until a complete update succeeds.
 logged_errors=0;
 bus.write_failures[2]=6;gpu.DeviceUpdateLEDs();gpu.DeviceUpdateLEDs();
 assert(bus.writes.empty() && logged_errors==1);
 bus.write_failures[7]=3;gpu.DeviceUpdateMode();assert(bus.writes.empty() && logged_errors==1);
 gpu.DeviceUpdateLEDs();assert(bus.writes.size()==4 && logged_errors==1);bus.writes.clear();
 bus.write_failures[2]=3;gpu.DeviceUpdateLEDs();assert(bus.writes.empty() && logged_errors==2);
 gpu.DeviceUpdateMode();assert(bus.writes.size()==5 && logged_errors==2);bus.writes.clear();
 bus.write_failures[7]=6;gpu.DeviceUpdateMode();gpu.DeviceUpdateMode();
 assert(bus.writes.empty() && logged_errors==3);
 for(int state:{0,1,0xac}){bus.regs[7]=state;RGBController_GalaxGPUv1 fallback(new GalaxGPUv1Controller(&bus,0x23,"GPU",true));assert(fallback.active_mode==(state==1?1:0));}
 bus.failures[7]=3;{RGBController_GalaxGPUv1 fallback(new GalaxGPUv1Controller(&bus,0x23,"GPU",true));assert(fallback.active_mode==0);}
 bus.failures[2]=3;auto low=std::make_unique<GalaxGPUv1Controller>(&bus,0x23,"GPU",true);bool rejected=false;unsigned int shutdown=RGBController::shutdown_calls;
 try{RGBController_GalaxGPUv1 failed(low.get());}catch(...){rejected=true;}
 assert(rejected && RGBController::shutdown_calls==shutdown+1);
 std::cout<<"Galax v1: legacy commands/modes retained; EX detection, commits, sync, bounds and failure checks passed\n";
}
