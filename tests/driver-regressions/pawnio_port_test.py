"""Exercise the actual port-select function and registration blocks without hardware."""
import argparse
import pathlib
import subprocess
import tempfile


def block(source, marker, start=0):
    begin = source.index(marker, start)
    opening = source.index("{", begin)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin:end], end


STUBS = r'''
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using HANDLE = void*;
using SIZE_T = std::size_t;
using ULONG64 = std::uint64_t;
using HRESULT = int;
using DWORD = unsigned int;
using s32 = int;
constexpr int S_OK = 0;
constexpr int FALSE = 0;
constexpr DWORD INFINITE = 0xFFFFFFFF;
constexpr DWORD WAIT_OBJECT_0 = 0;
constexpr DWORD WAIT_ABANDONED = 0x80;
#define GLOBAL_SMBUS_MUTEX_NAME "Global\\Access_SMBUS.HTP.Method"
#define LOG_WARNING(...) ((void)0)
bool held = false;
bool create_failure = false;
DWORD wait_result = 0;
int port_result[2] = {0,0};
int executes = 0, releases = 0, closes = 0, module_closes = 0, loads = 0;
std::vector<unsigned int> registered;
HANDLE CreateMutexA(void*, int, const char*) {return create_failure ? nullptr : (HANDLE)1;}
DWORD WaitForSingleObject(HANDLE, DWORD) {held = wait_result == 0 || wait_result == 0x80; return wait_result;}
int ReleaseMutex(HANDLE) {assert(held);held=false;++releases;return 1;}
int CloseHandle(HANDLE) {++closes;return 1;}
HRESULT pawnio_execute(HANDLE, const char* name, ULONG64* in, SIZE_T size, ULONG64*, SIZE_T, SIZE_T*)
{
    assert(held);assert(std::strcmp(name,"ioctl_piix4_port_sel")==0);assert(size==1);
    ++executes;assert(in[0]<=1);return port_result[in[0]];
}
int pawnio_close(HANDLE) {++module_closes;return 0;}
struct i2c_smbus_interface {unsigned int index;};
struct i2c_smbus_pawnio : i2c_smbus_interface
{
    i2c_smbus_pawnio(HANDLE,const char*,unsigned int port) {index=port;}
    static int start_pawnio(const char*,HANDLE* handle) {*handle=(HANDLE)(std::uintptr_t)(100+loads++);return S_OK;}
};
struct DetectionManager
{
    static DetectionManager* get(){static DetectionManager manager;return &manager;}
    void RegisterI2CBus(i2c_smbus_interface* bus){registered.push_back(bus->index);delete bus;}
};
'''

MAIN = r'''
int main()
{
    assert(piix4_port_sel((HANDLE)10,0)==0);
    assert(executes==1 && releases==1 && closes==1 && !held);
    wait_result=WAIT_ABANDONED;
    assert(piix4_port_sel((HANDLE)10,0)==0);
    wait_result=0;
    port_result[1]=-1;
    assert(piix4_port_sel((HANDLE)11,1)==-EIO);
    assert(!held && releases==3 && closes==3);
    wait_result=0xFFFFFFFF;
    assert(piix4_port_sel((HANDLE)10,0)==-EIO && executes==3 && closes==4);
    create_failure=true;
    assert(piix4_port_sel((HANDLE)10,0)==-EIO && executes==3 && closes==4);
    create_failure=false;wait_result=0;
    assert(detect_ports());
    assert(registered==std::vector<unsigned int>{0} && module_closes==1);
    registered.clear();port_result[0]=-1;port_result[1]=0;
    assert(detect_ports());
    assert(registered==std::vector<unsigned int>{1} && module_closes==2);
    registered.clear();port_result[1]=-1;
    assert(!detect_ports() && registered.empty() && module_closes==4);
    std::cout << "PawnIO port locking, failed selection cleanup and correct interface registration pass\n";
}
'''

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source")
    parser.add_argument("compiler")
    parser.add_argument("output")
    args = parser.parse_args()
    source = pathlib.Path(args.source).read_text(encoding="utf-8-sig")
    function, _ = block(source, "s32 piix4_port_sel(")
    marker = 'if(i2c_smbus_pawnio::start_pawnio("SmbusPIIX4.bin"'
    primary, end = block(source, marker)
    secondary, _ = block(source, marker, end)
    output = pathlib.Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    cpp = output / "pawnio-ports.cpp"
    exe = output / "pawnio-ports.exe"
    cpp.write_text(STUBS + function + '\nbool detect_ports(){bool bus_detected=false;HANDLE pawnio_handle;'
                   'i2c_smbus_interface* bus;\n' + primary + secondary + '\nreturn bus_detected;}\n' + MAIN)
    subprocess.run([args.compiler, "-std=c++17", str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
