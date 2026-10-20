#include "BatteryStatus/WindowsHidTransfer.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#ifdef _WIN32
namespace {
HANDLE device_handle=reinterpret_cast<HANDLE>(1), event_handle=reinterpret_cast<HANDLE>(2);
std::vector<DWORD> waits;
std::vector<HANDLE> closed;
unsigned int wait_index=0, cancels=0;
DWORD last_count=64;
bool immediate=false, writing=false;
unsigned char* held_buffer=nullptr;
OVERLAPPED* held_operation=nullptr;
HANDLE WINAPI CreateEventFake(LPSECURITY_ATTRIBUTES,BOOL,BOOL,LPCWSTR) { return event_handle; }
BOOL WINAPI IoFake(HANDLE device,LPVOID buffer,DWORD count,LPDWORD transferred,LPOVERLAPPED operation) {
    assert(device==device_handle && (count==64 || count==128));last_count=count;
    if(count==128) { assert(operation->Offset==64 && operation->OffsetHigh==1); }
    held_buffer=static_cast<unsigned char*>(buffer); held_operation=operation;
    if(!writing) { std::memset(buffer,0x59,count); }
    *transferred=last_count;
    return immediate;
}
BOOL WINAPI WriteFake(HANDLE d,LPCVOID b,DWORD n,LPDWORD t,LPOVERLAPPED o) { return IoFake(d,const_cast<LPVOID>(b),n,t,o); }
DWORD WINAPI ErrorFake() { return ERROR_IO_PENDING; }
DWORD WINAPI WaitFake(HANDLE event,DWORD timeout) {
    assert(event==event_handle && timeout!=INFINITE);
    assert(wait_index<waits.size()); return waits[wait_index++];
}
BOOL WINAPI CancelFake(HANDLE device,LPOVERLAPPED operation) {
    assert(device==device_handle && operation==held_operation); ++cancels; return TRUE;
}
BOOL WINAPI ResultFake(HANDLE device,LPOVERLAPPED operation,LPDWORD transferred,BOOL wait) {
    assert(device==device_handle && operation==held_operation && wait==FALSE); *transferred=last_count; return TRUE;
}
BOOL WINAPI CloseFake(HANDLE value) { closed.push_back(value); return TRUE; }
WindowsHidTransfer::Api FakeApi() {
    return {CreateEventFake,IoFake,WriteFake,ErrorFake,WaitFake,CancelFake,ResultFake,CloseFake};
}
void Reset(std::vector<DWORD> states,bool immediate_value=false,bool write=false) {
    waits=std::move(states);wait_index=0;cancels=0;closed.clear();immediate=immediate_value;writing=write;
}
}
int main() {
    unsigned char bytes[64]={}; HANDLE device=device_handle;
    Reset({},true);
    { WindowsHidTransfer io(FakeApi()); assert(io.Transfer(device,false,bytes,64,250)==64); assert(bytes[0]==0x59); }
    assert(cancels==0 && closed.size()==1 && closed[0]==event_handle);
    Reset({WAIT_OBJECT_0});
    { WindowsHidTransfer io(FakeApi()); assert(io.Transfer(device,false,bytes,64,250)==64); }
    assert(cancels==0);
    Reset({WAIT_TIMEOUT,WAIT_OBJECT_0});
    { WindowsHidTransfer io(FakeApi()); assert(io.Transfer(device,false,bytes,64,250)==0); assert(io.Ready()); }
    assert(cancels==1 && device==device_handle && closed.size()==1);
    Reset({WAIT_TIMEOUT,WAIT_TIMEOUT,WAIT_TIMEOUT,WAIT_OBJECT_0},false,true);
    {
        WindowsHidTransfer io(FakeApi()); bytes[0]=0xCC;
        assert(io.Transfer(device,true,bytes,64,100)==-1);
        assert(device==INVALID_HANDLE_VALUE && cancels==1 && closed.empty());
        // Caller storage can disappear while the driver still owns its own copy.
        bytes[0]=0; assert(held_buffer!=bytes && held_buffer[0]==0xCC);
        assert(held_operation->hEvent==event_handle);
        assert(!io.Ready() && closed.empty());
        assert(io.Ready()); // Cleanup only after Windows signals completion.
        assert(closed.size()==2);
    }
    device=device_handle;
    Reset({WAIT_TIMEOUT,WAIT_TIMEOUT,WAIT_TIMEOUT},false,true);
    {
        WindowsHidTransfer io(FakeApi()); bytes[0]=0xCC;
        assert(io.Transfer(device,true,bytes,64,100)==-1);
    }
    assert(device==INVALID_HANDLE_VALUE && closed.empty());
    assert(held_buffer[0]==0xCC && held_operation->hEvent==event_handle);
    device=device_handle;Reset({},true);
    {
        WindowsHidTransfer io(FakeApi());unsigned char tail[128]={};
        assert(io.Transfer(device,false,tail,65,50)==-1);
        assert(io.ReadAt(device,tail,128,50,0x100000040ULL)==128);
        assert(tail[127]==0x59);
    }
    std::cout<<"Windows HID: immediate/pending completion, bounded cancellation and retained pending I/O ownership passed\n";
}
#else
int main() { return 0; }
#endif
