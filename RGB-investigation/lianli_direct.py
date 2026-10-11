"""SL-Infinity lighting only: vendor feature commands + output colour data."""
import argparse
import ctypes as c
from pathlib import Path
import time
import aura_hid as hid

ROOT = Path(__file__).resolve().parent

def validate_capture():
    reports = {}
    for line in (ROOT / 'LConnect-sync-payloads.tsv').read_text(encoding='utf-8-sig').splitlines():
        fields = line.split('\t')
        if len(fields) >= 3 and fields[2]:
            reports[int(fields[0])] = bytes.fromhex(fields[2])
    for frame, expected in [(23, 'e0106001030000'), (33, 'e0170100000000')]:
        if reports.get(frame) != bytes.fromhex(expected):
            raise RuntimeError('Expected vendor lighting command missing from capture')
    for channel in range(8):
        if not any(len(p) == 353 and p[:2] == bytes([0xe0, 0x30 + channel]) for p in reports.values()):
            raise RuntimeError('Missing captured output colour channel')
    return reports

def colour_report(channel, rgb, fans):
    if not 0 <= channel < 8 or not 1 <= fans <= 4:
        raise ValueError('Invalid lighting channel/fan count')
    r, g, b = rgb
    return bytes([0xe0, 0x30 + channel]) + bytes([r, b, g]) * (16 * fans) + bytes(351 - 48 * fans)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--replay', action='store_true')
    parser.add_argument('--fans', default='3,2,1,0', help='Fans on each physical hub port, confirmed before replay')
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    counts = [int(n) for n in args.fans.split(',')]
    if len(counts) != 4 or any(n < 0 or n > 4 for n in counts):
        raise ValueError('Specify four fan counts from 0 to 4')
    reports = validate_capture()
    print('Vendor static commands and all eight output colour channels validated.', flush=True)
    if args.offline:
        return
    paths = list(hid.device_paths(0x0cf2, 0xa102))
    if len(paths) != 1:
        raise RuntimeError(f'Expected one SL-Infinity interface, found {len(paths)}')
    handle = hid.open_device(paths[0])
    try:
        caps = hid.capabilities(handle)
        if (caps.page, caps.usage, caps.output, caps.feature) != (0xff72, 0xa1, 353, 7):
            raise RuntimeError('Unexpected hub report capabilities; no writes sent')
        print('Confirmed SL-Infinity interface: output 353 bytes, feature 7 bytes.', flush=True)
        if not args.replay:
            print('Read-only inspection completed; no writes sent.', flush=True)
            return
        hid.hid.HidD_SetFeature.argtypes = [hid.w.HANDLE, c.c_void_p, hid.w.DWORD]
        def send(data, feature):
            if data[0] != 0xe0 or len(data) != (7 if feature else 353):
                raise RuntimeError('Unexpected report')
            # Explicit lighting allowlist. Never send PWM commands 0x20-0x23,
            # RPM sync 0x62, merge configuration, or firmware commands.
            if feature:
                valid = data[1:3] == b'\x10\x60' or (0x10 <= data[1] <= 0x17 and data[2] == 1)
            else:
                valid = 0x30 <= data[1] <= 0x37
            if not valid:
                raise RuntimeError('Non-lighting command blocked')
            buffer = c.create_string_buffer(data, len(data))
            call = hid.hid.HidD_SetFeature if feature else hid.hid.HidD_SetOutputReport
            if not call(handle, buffer, len(data)):
                raise c.WinError(c.get_last_error())
            time.sleep(0.03)
        for label, rgb in [('RED', (255, 0, 0)), ('GREEN', (0, 255, 0)),
                           ('BLUE', (0, 0, 255)), ('PURPLE', (128, 0, 255))]:
            print(f'{label}: watch fan centre and side lighting.', flush=True)
            for port, fans in enumerate(counts):
                if not fans:
                    continue
                send(bytes([0xe0, 0x10, 0x60, port + 1, fans, 0, 0]), True)
                for channel in (port * 2, port * 2 + 1):
                    send(colour_report(channel, rgb, fans), False)
                    # Exactly the captured vendor static effect commit.
                    commit = bytes([0xe0, 0x10 + channel, 1, 0, 0, 0, 0])
                    if commit not in reports.values():
                        raise RuntimeError('Uncaptured static commit blocked')
                    send(commit, True)
            time.sleep(5)
        print('Lighting test finished; final requested colour is purple. Confirm physical response.', flush=True)
    finally:
        hid.kernel.CloseHandle(handle)

if __name__ == '__main__':
    main()