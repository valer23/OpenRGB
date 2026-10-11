"""Replay the exact SL-Infinity feature report captured from L-Connect."""
import ctypes as c
from pathlib import Path
import aura_hid as hid

def main():
    expected = bytes.fromhex('e0106101000000')
    recorded = []
    for line in (Path(__file__).parent / 'LConnect-sync-payloads.tsv').read_text(encoding='utf-8-sig').splitlines():
        parts = line.split('\t')
        if len(parts) >= 3 and parts[2]:
            recorded.append(bytes.fromhex(parts[2]))
    if expected not in recorded:
        raise RuntimeError('Exact vendor sync report missing from capture; refusing write')
    paths = list(hid.device_paths(0x0CF2, 0xA102))
    if len(paths) != 1:
        raise RuntimeError(f'Expected one SL-Infinity HID interface, found {len(paths)}')
    handle = hid.open_device(paths[0])
    try:
        caps = hid.capabilities(handle)
        if (caps.page, caps.usage, caps.feature) != (0xFF72, 0xA1, 7):
            raise RuntimeError('Unexpected hub capabilities; refusing write')
        hid.hid.HidD_SetFeature.argtypes = [hid.w.HANDLE, c.c_void_p, hid.w.DWORD]
        buffer = c.create_string_buffer(expected, len(expected))
        if not hid.hid.HidD_SetFeature(handle, buffer, len(expected)):
            raise c.WinError(c.get_last_error())
        print('Exact captured 7-byte motherboard lighting-sync feature report sent successfully.', flush=True)
    finally:
        hid.kernel.CloseHandle(handle)

if __name__ == '__main__':
    main()
