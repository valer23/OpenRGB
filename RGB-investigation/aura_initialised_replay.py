"""Replay only the captured lighting setup and colour reports; default is dry run."""
import argparse
import ctypes as c
from pathlib import Path
import time

import aura_hid as transport


def setup_reports():
    rows = {}
    for line in (Path(__file__).parent / 'Aura-startup-bidirectional.tsv').read_text().splitlines():
        fields = line.split('\t')
        if len(fields) >= 3 and fields[2]:
            rows[int(fields[0])] = bytes.fromhex(fields[2])
    # Keep the captured order. Exclude all firmware/configuration reads and
    # the unidentified 0x23/0x28 commands. Frame 407 is the captured
    # post-setup command before the first colour update; its exact purpose
    # is still unverified. The first test omitted it and no lights changed.
    frames = (367, 369, 371, 373, 381, 383, 391, 393, 407)
    expected_prefixes = (
        '410500000000000001', '410501000000000001',
        '410501000000010001', '4104000078',
        '410500000100000001', '4104010078',
        '410500000200000001', '4104020078',
        '41210001',
    )
    reports = []
    for frame, prefix in zip(frames, expected_prefixes):
        report = rows[frame]
        expected = bytes.fromhex(prefix).ljust(64, b'\x00')
        if report != expected:
            raise RuntimeError(f'Unexpected startup report at frame {frame}; refusing replay')
        reports.append(report)
    return reports


def send(handle, report):
    buffer = c.create_string_buffer(report, len(report))
    if not transport.hid.HidD_SetOutputReport(handle, buffer, len(report)):
        raise c.WinError(c.get_last_error())
    time.sleep(0.03)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--send', action='store_true')
    args = parser.parse_args()
    initialisation = setup_reports()
    cycles = transport.captured_cycles()
    colours = [('RED', b'\x00\xff\x00'), ('GREEN', b'\xff\x00\x00'),
               ('BLUE', b'\x00\x00\xff'), ('RED', b'\x00\xff\x00')]
    for name, colour in colours:
        if len(cycles[colour]) != 26:
            raise RuntimeError(f'Missing complete {name} cycle')
    print('Validated 9 exact captured lighting setup reports (including post-setup command) and four 26-report colour cycles.', flush=True)
    if not args.send:
        print('Dry run: no hardware accessed.', flush=True)
        return
    paths = list(transport.device_paths())
    if len(paths) != 1:
        raise RuntimeError(f'Expected one ASUS HID interface, found {len(paths)}')
    handle = transport.open_device(paths[0])
    try:
        caps = transport.capabilities(handle)
        if (caps.page, caps.usage, caps.input, caps.output, caps.feature) != (0xFF0A, 1, 64, 64, 0):
            raise RuntimeError('Unexpected ASUS HID capabilities; refusing replay')
        print('Sending captured lighting setup (existing 120 LEDs per header).', flush=True)
        for report in initialisation[:-1]:
            send(handle, report)
        # Match the service's pause before frame 407, then its pause before
        # the first colour report. Do not infer or synthesize any commands.
        time.sleep(2.2)
        send(handle, initialisation[-1])
        time.sleep(1)
        for name, colour in colours:
            print(f'{name}: watch motherboard logo, fan lights and GPU lights.', flush=True)
            for report in cycles[colour]:
                send(handle, report)
            time.sleep(5)
        print('All setup and colour reports delivered. Physical response still needs confirmation.', flush=True)
    finally:
        transport.kernel.CloseHandle(handle)


if __name__ == '__main__':
    main()
