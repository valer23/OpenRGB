"""Read HID capabilities, or replay only exact lighting reports from our capture."""
import argparse
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import time

class GUID(c.Structure):
    _fields_ = [('a', w.DWORD), ('b', w.WORD), ('d', w.WORD), ('e', c.c_ubyte * 8)]

class Interface(c.Structure):
    _fields_ = [('size', w.DWORD), ('guid', GUID), ('flags', w.DWORD), ('reserved', c.c_size_t)]

class Caps(c.Structure):
    _fields_ = [(n, w.USHORT) for n in ('usage', 'page', 'input', 'output', 'feature')] + [('reserved', w.USHORT * 17)] + [(f'count{i}', w.USHORT) for i in range(10)]

setup = c.WinDLL('setupapi', use_last_error=True)
hid = c.WinDLL('hid', use_last_error=True)
kernel = c.WinDLL('kernel32', use_last_error=True)
setup.SetupDiGetClassDevsW.argtypes = [c.POINTER(GUID), w.LPCWSTR, w.HWND, w.DWORD]
setup.SetupDiGetClassDevsW.restype = w.HANDLE
setup.SetupDiEnumDeviceInterfaces.argtypes = [w.HANDLE, c.c_void_p, c.POINTER(GUID), w.DWORD, c.POINTER(Interface)]
setup.SetupDiGetDeviceInterfaceDetailW.argtypes = [w.HANDLE, c.POINTER(Interface), c.c_void_p, w.DWORD, c.POINTER(w.DWORD), c.c_void_p]
setup.SetupDiDestroyDeviceInfoList.argtypes = [w.HANDLE]
kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p, w.DWORD, w.DWORD, w.HANDLE]
kernel.CreateFileW.restype = w.HANDLE
kernel.CloseHandle.argtypes = [w.HANDLE]
hid.HidD_GetHidGuid.argtypes = [c.POINTER(GUID)]
hid.HidD_GetPreparsedData.argtypes = [w.HANDLE, c.POINTER(c.c_void_p)]
hid.HidD_FreePreparsedData.argtypes = [c.c_void_p]
hid.HidP_GetCaps.argtypes = [c.c_void_p, c.POINTER(Caps)]
hid.HidD_SetOutputReport.argtypes = [w.HANDLE, c.c_void_p, w.DWORD]

def device_paths(vendor=0x0B05, product=0x1CD2):
    guid = GUID()
    hid.HidD_GetHidGuid(c.byref(guid))
    info = setup.SetupDiGetClassDevsW(c.byref(guid), None, None, 0x12)
    if info == c.c_void_p(-1).value:
        raise c.WinError(c.get_last_error())
    try:
        index = 0
        while True:
            interface = Interface()
            interface.size = c.sizeof(interface)
            if not setup.SetupDiEnumDeviceInterfaces(info, None, c.byref(guid), index, c.byref(interface)):
                if c.get_last_error() != 259:
                    raise c.WinError(c.get_last_error())
                break
            size = w.DWORD()
            setup.SetupDiGetDeviceInterfaceDetailW(info, c.byref(interface), None, 0, c.byref(size), None)
            detail = c.create_string_buffer(size.value)
            c.cast(detail, c.POINTER(w.DWORD))[0] = 8 if c.sizeof(c.c_void_p) == 8 else 6
            if not setup.SetupDiGetDeviceInterfaceDetailW(info, c.byref(interface), detail, size, None, None):
                raise c.WinError(c.get_last_error())
            path = c.wstring_at(c.addressof(detail) + 4)
            if f'vid_{vendor:04x}&pid_{product:04x}' in path.lower():
                yield path
            index += 1
    finally:
        setup.SetupDiDestroyDeviceInfoList(info)

def open_device(path):
    handle = kernel.CreateFileW(path, 0, 3, None, 3, 0, None)
    if handle == c.c_void_p(-1).value:
        raise c.WinError(c.get_last_error())
    return handle

def capabilities(handle):
    data = c.c_void_p()
    if not hid.HidD_GetPreparsedData(handle, c.byref(data)):
        raise c.WinError(c.get_last_error())
    try:
        caps = Caps()
        status = hid.HidP_GetCaps(data, c.byref(caps))
        if status != 0x110000:
            raise RuntimeError(f'HID capabilities status: {status:#x}')
        return caps
    finally:
        hid.HidD_FreePreparsedData(data)

def captured_cycles():
    reports = [bytes.fromhex(line.split('\t')[-1]) for line in (Path(__file__).parent / 'Aura-payloads.tsv').read_text(encoding='utf-8-sig').splitlines() if line]
    found = {}
    for start in range(0, len(reports), 26):
        cycle = reports[start:start + 26]
        if len(cycle) != 26 or any(len(p) != 64 or p[:2] != b'\x41\x05' for p in cycle):
            raise RuntimeError('Unexpected capture layout; refusing replay')
        colour = cycle[0][18:21]
        if any(p[18:21] != colour for p in cycle):
            raise RuntimeError('Mixed colour cycle; refusing replay')
        found.setdefault(colour, cycle)
    return found

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--replay', action='store_true', help='Send captured red/green/blue/red cycles')
    args = parser.parse_args()
    paths = list(device_paths())
    if not paths:
        raise RuntimeError('ASUS VID 0B05 PID 1CD2 HID interface not found')
    if len(paths) != 1:
        raise RuntimeError(f'Expected one HID interface, found {len(paths)}')
    handle = open_device(paths[0])
    try:
        caps = capabilities(handle)
        print('ASUS HID:', paths[0], flush=True)
        print(f'Usage: {caps.page:04X}:{caps.usage:04X}; Input={caps.input}, Output={caps.output}, Feature={caps.feature}', flush=True)
        if args.replay:
            if (caps.page, caps.usage, caps.output) != (0xFF0A, 1, 64):
                raise RuntimeError('Unexpected capabilities; refusing replay')
            cycles = captured_cycles()
            for name, colour in [('RED', b'\x00\xff\x00'), ('GREEN', b'\xff\x00\x00'), ('BLUE', b'\x00\x00\xff'), ('RED', b'\x00\xff\x00')]:
                print(f'Sending captured {name} reports. Watch motherboard, fans, GPU.', flush=True)
                for report in cycles[colour]:
                    buffer = c.create_string_buffer(report, len(report))
                    if not hid.HidD_SetOutputReport(handle, buffer, len(report)):
                        raise c.WinError(c.get_last_error())
                    time.sleep(0.003)
                time.sleep(5)
            print('Replay completed successfully.', flush=True)
    finally:
        kernel.CloseHandle(handle)

if __name__ == '__main__':
    main()
