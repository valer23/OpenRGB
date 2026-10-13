#include "RGBController_KFA2EX.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
unsigned int logged_errors=0;
struct Write { unsigned char reg; std::vector<unsigned char> bytes; };
struct Bus : i2c_smbus_interface {
 unsigned char regs[16]={0x11,0,255,0,0,0,0x20,1};
 unsigned int read_failures[16]={},write_failures[16]={},reads[16]={};
 bool short_reads=false;
 std::vector<Write> writes;
 Bus(){std::strcpy(info.device_name,"Mock NVIDIA I2C");}
 s32 i2c_xfer(u8,char,int*,u8*) override {return -1;}
 s32 i2c_smbus_xfer(u8 address,char rw,u8 reg,int mode,i2c_smbus_data* data) override {
  assert(address==0x23 && reg<16);
  assert(mode==I2C_SMBUS_I2C_BLOCK_DATA || mode==I2C_SMBUS_BYTE_DATA);
  unsigned int count=mode==I2C_SMBUS_BYTE_DATA?1:data->block[0];
  assert(reg+count<=16);
  if(rw==I2C_SMBUS_READ){++reads[reg];if(read_failures[reg]){--read_failures[reg];return -1;}
   assert(mode==I2C_SMBUS_I2C_BLOCK_DATA);std::memcpy(&data->block[1],regs+reg,count);if(short_reads)data->block[0]=count-1;return 0;}
  if(write_failures[reg]){--write_failures[reg];return -1;}
  unsigned char* bytes=mode==I2C_SMBUS_BYTE_DATA?&data->byte:&data->block[1];
  writes.push_back({reg,{bytes,bytes+count}});std::memcpy(regs+reg,bytes,count);return 0;
 }
};
int main(){
 Bus bus;bus.read_failures[0]=2;assert(RGBController_KFA2EX::Probe(&bus,0x23));assert(bus.reads[0]==3);
 bus.regs[0]=0x27;assert(!RGBController_KFA2EX::Probe(&bus,0x23));bus.regs[0]=0x11;
 bus.short_reads=true;assert(!RGBController_KFA2EX::Probe(&bus,0x23));bus.short_reads=false;
 RGBController_KFA2EX driver(&bus,0x23,"KFA2 GeForce RTX 3080 Ti EX Gamer");
 assert(bus.writes.empty());assert(driver.active_mode==1);
 assert(driver.location=="I2C: Mock NVIDIA I2C, address 0x23");
 driver.DeviceUpdateLEDs();assert(bus.writes.empty());
 driver.active_mode=0;bus.write_failures[7]=1;driver.DeviceUpdateMode();assert(bus.writes.size()==5);
 assert(bus.writes[0].reg==7 && bus.writes[0].bytes==std::vector<unsigned char>{0});
 bus.writes.clear();driver.colors[0]=ToRGBColor(0,255,0);driver.DeviceUpdateLEDs();
 assert(bus.writes.size()==4);assert(bus.writes[0].reg==2 && bus.writes[0].bytes==std::vector<unsigned char>({0,255,0}));
 assert(bus.writes[1].reg==5 && bus.writes[1].bytes[0]==0);
 assert(bus.writes[2].reg==6 && bus.writes[2].bytes[0]==0x20);
 assert(bus.writes[3].reg==8 && bus.writes[3].bytes[0]==0x5a);
 bus.writes.clear();bus.write_failures[2]=3;driver.DeviceUpdateLEDs();assert(bus.writes.empty());assert(logged_errors);
 driver.DeviceUpdateZoneLEDs(-1);driver.DeviceUpdateSingleLED(1);assert(bus.writes.empty());
 bus.regs[7]=0xac;{RGBController_KFA2EX fallback(&bus,0x23,"GPU");assert(fallback.active_mode==0);}
 bus.read_failures[2]=3;unsigned int shutdown=RGBController::shutdown_calls;bool rejected=false;
 try{RGBController_KFA2EX fail(&bus,0x23,"GPU");}catch(...){rejected=true;}
 assert(rejected && RGBController::shutdown_calls==shutdown+1);
 std::cout<<"Shared SMBus GPU: read-only probe, exact colour commits, sync, short reads, failure bounds and callback checks passed\n";
}
