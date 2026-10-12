// Compile the actual NVAPI implementation; never initialize/load the vendor DLL.
#include "../../dependencies/NVFC/nvapi.cpp"
#include <cassert>
#include <iostream>
NV_PHYSICAL_GPU_HANDLE expected_handle=nullptr;
NV_STATUS requested_status=0;
NV_STATUS fake_bus(NV_PHYSICAL_GPU_HANDLE handle,NV_U32* output) {
    assert(handle==expected_handle); *output=7; return requested_status;
}
NV_STATUS fake_slot(NV_PHYSICAL_GPU_HANDLE handle,NV_U32* output) {
    assert(handle==expected_handle); *output=3; return requested_status;
}
void* fake_query(int id) {
    if(static_cast<NV_U32>(id)==0x1BE0B8E5) return reinterpret_cast<void*>(&fake_bus);
    if(static_cast<NV_U32>(id)==0x2A0A350F) return reinterpret_cast<void*>(&fake_slot);
    return nullptr;
}
int main() {
    NV_I2C_INFO_V3 info;
    assert(sizeof(info)==64 && info.version==(64|(3<<16)));
    assert(info.display_mask==0 && info.data==nullptr && info.i2c_reg_address==nullptr);
    NV_U32 value=123;
    assert(NvAPI_GPU_GetBusId(nullptr,&value)==-1 && value==123);
    assert(NvAPI_GPU_GetBusSlotId(nullptr,&value)==-1 && value==123);
    QueryInterfaces(fake_query);
    NV_S32 handle=1; expected_handle=&handle;
    assert(NvAPI_GPU_GetBusId(&handle,&value)==0 && value==7);
    assert(NvAPI_GPU_GetBusSlotId(&handle,&value)==0 && value==3);
    requested_status=-6;
    assert(NvAPI_GPU_GetBusId(&handle,&value)==-6);
    assert(NvAPI_GPU_GetBusSlotId(&handle,&value)==-6);
    std::cout<<"Real NVAPI ABI initialization, PCI query IDs, unavailable functions and error propagation passed.\n";
}
