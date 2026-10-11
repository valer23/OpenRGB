#include "RGBController_KFA2EX_Windows.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

unsigned int logged_errors=0;
unsigned char registers[16]={0x11,0,255,0,0,0,0x20,1};
unsigned int sync_failures=0;
bool fail_bus=false,fail_slot=false;
NV_STATUS NvAPI_GPU_GetBusId(NV_PHYSICAL_GPU_HANDLE gpu,NV_U32* bus) {if(fail_bus)return -1; *bus=gpu && *gpu!=2?2:1; return 0;}
NV_STATUS NvAPI_GPU_GetBusSlotId(NV_PHYSICAL_GPU_HANDLE gpu,NV_U32* slot) {if(fail_slot)return -1; *slot=gpu && *gpu==2?1:0; return 0;}
struct Call { unsigned char reg; std::vector<unsigned char> data; };
std::vector<Call> writes;

// Implement the real NVAPI seam; never load the NVIDIA DLL or access hardware.
NV_I2C_INFO_V3::NV_I2C_INFO_V3() {
    std::memset(this,0,sizeof(*this));version=sizeof(*this)|(3<<16);
}
void check_transport(NV_I2C_INFO_V3* info) {
    assert(sizeof(*info)==64 && info->version==(64|(3<<16)));
    assert(info->display_mask==1 && info->is_ddc_port==2);
    assert(info->i2c_dev_address==0x46 && info->reg_addr_size==1);
    assert(info->port_id==1 && info->is_port_id_set==1);
    assert(info->i2c_speed==0 && static_cast<int>(info->i2c_speed_khz)==0);
}
NV_STATUS NvAPI_I2CRead(NV_PHYSICAL_GPU_HANDLE,NV_I2C_INFO_V3* info) {
    check_transport(info);
    std::memcpy(info->data,registers+*info->i2c_reg_address,info->size);return 0;
}
NV_STATUS NvAPI_I2CWrite(NV_PHYSICAL_GPU_HANDLE,NV_I2C_INFO_V3* info) {
    check_transport(info);
    auto reg=*info->i2c_reg_address;
    if(reg==7 && sync_failures) { --sync_failures;return -1; }
    writes.push_back({reg,{info->data,info->data+info->size}});
    std::memcpy(registers+reg,info->data,info->size);return 0;
}

int main() {
    assert(RGBController_KFA2EX::Probe(nullptr));
    registers[0]=0x27;
    assert(!RGBController_KFA2EX::Probe(nullptr));
    registers[0]=0x11;
    RGBController_KFA2EX driver(nullptr);
    NV_S32 second_handle=1;
    RGBController_KFA2EX second(&second_handle);
    NV_S32 third_handle=2;
    RGBController_KFA2EX same_bus_other_slot(&third_handle);
    RGBController_KFA2EX same_card_again(nullptr);
    assert(driver.location==same_card_again.location);
    assert(driver.location!=same_bus_other_slot.location);
    assert(second.location!=same_bus_other_slot.location);
    assert(driver.location!=second.location);
    assert(driver.location=="NVAPI: PCI bus 1, slot 0, I2C address 0x23, port 1");
    for(bool bus_failure : {true,false}) {
        fail_bus=bus_failure; fail_slot=!bus_failure;
        bool rejected=false;
        try { RGBController_KFA2EX unidentified(nullptr); } catch(const std::exception&) {rejected=true;}
        assert(rejected);
    }
    fail_bus=false; fail_slot=false;
    assert(writes.empty()); // Detection/constructor must remain read-only.
    assert(driver.active_mode==1);
    driver.DeviceUpdateLEDs();assert(writes.empty()); // Ignore colours in MB mode.
    driver.active_mode=0;sync_failures=1;
    driver.DeviceUpdateMode();
    assert(writes.size()==5 && writes[0].reg==7 && writes[0].data[0]==0);
    writes.clear();
    driver.colors[0]=ToRGBColor(0,255,0);
    driver.DeviceUpdateLEDs();
    // Exact successful GREEN sequence from the vendor capture.
    assert(writes.size()==4);
    assert(writes[0].reg==2 && writes[0].data==std::vector<unsigned char>({0,255,0}));
    assert(writes[1].reg==5 && writes[1].data==std::vector<unsigned char>({0}));
    assert(writes[2].reg==6 && writes[2].data==std::vector<unsigned char>({0x20}));
    assert(writes[3].reg==8 && writes[3].data==std::vector<unsigned char>({0x5A}));
    writes.clear();driver.active_mode=1;driver.DeviceUpdateMode();
    assert(writes.size()==1 && writes[0].reg==7 && writes[0].data[0]==1);
    writes.clear();driver.active_mode=0;sync_failures=3;driver.DeviceUpdateMode();
    assert(writes.empty() && sync_failures==0 && logged_errors==1);
    std::cout<<"Native KFA2 transport, captured colour sequence, read-only detection, sync retry and bounded failure checks passed.\n";
}
