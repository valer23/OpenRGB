# ASUS 0B05:1CD2 lighting investigation

Hardware: ROG STRIX B850-F GAMING WIFI7 NEO. Evidence: local capture
`Aura-colours-20261011-145653.pcap`, 1,982 packets, 988 output payloads.

## Verified transport

- HID interface MI_01, usage page FF0A, usage 0001.
- Windows HID capabilities: input 64 bytes, output 64 bytes, no feature reports.
- Captured requests: class SET_REPORT, bmRequestType 21, bRequest 09,
  wValue 0241, wIndex 0001, wLength 0040.
- All lighting payloads are 64 bytes, beginning with report ID 41 and command 05.
- OpenRGB checkout 4d17bac does not register product 1CD2. Its existing ASUS
  motherboard implementation uses a different EC protocol. Adding this ID to
  that detector alone is not supported by the evidence.

## Observed report layout

Offsets below are zero-based and include the report ID.

| Offset | Captured value | Interpretation |
|---|---|---|
| 0 | 41 | HID output report ID |
| 1 | 05 | Colour update command |
| 2 | 00 or 01 | 00 for addressable headers; 01 for onboard outputs (inferred) |
| 3 | 00 | Unknown |
| 4 | 00, 01, 02 | Addressable header index (inferred) |
| 5 | 00 | Unknown |
| 6 | 00, 01 | Onboard output index when byte 2 is 01 (inferred) |
| 7–13 | 00 | Unknown/reserved |
| 14–15 | 0000 through 0069 | LED start index, little endian (inferred) |
| 16–17 | 0001 or 000F | LED count, little endian (inferred) |
| 18 onward | Colour bytes | **Green, Red, Blue** triplets |

One full update is 26 reports: two onboard reports with one LED each, then
eight 15-LED chunks for each of three headers. This matches the configured
120 LEDs per header. Capture contains 38 complete updates and 78 unique
reports, corresponding to three colours times 26 reports.

Measured colour transitions: use `analyse_aura.py` for frame/time values.
The report byte order is GRB: red = 00 FF 00; green = FF 00 00;
blue = 00 00 FF. The first replay had an incorrect offset and stopped before
blue; the script was corrected to use bytes 18–20 and all three captured
cycles were validated offline before repeating the hardware test.

## Replay scope

`aura_hid.py` defaults to read-only enumeration/capability inspection.
`--replay` sends only exact reports extracted from this capture, in the
red/green/blue/red order. It checks VID/PID, HID usage, report length, complete
cycles, and consistent colour values. It sends no configuration, firmware,
fan-speed, or overclocking commands.

`Test-Aura-Replay.ps1` pauses LightingService for the test and restores it in
its finally block if it was previously running. No startup settings change.
Physical colour response must be confirmed by the user; successful Windows
report delivery alone does not prove the attached LEDs respond.

## Physical replay result

On 2026-10-11 the user confirmed that the corrected direct replay changed the
motherboard logo red → green → blue → red. GPU lighting and fan lighting did
not change. The log confirms all four cycles completed and LightingService
was restarted. This validates the motherboard USB colour command, but does
not validate the attached ARGB devices or demonstrate a cable fault.

## Still unresolved

## Startup capture, 2026-10-11

`Aura-startup-20261011-172131.pcap` records LightingService startup with
Fan Control left running: 760 packets, traffic spanning 10.03 seconds.
Bidirectional payloads are in `Aura-startup-bidirectional.tsv`. Output is
`usb.data_fragment`; interrupt input responses are `usbhid.data`.

New observations, not yet a complete protocol specification:

- Command 03 responses describe three header entries with 120 LEDs each.
- Command 04 reports at frames 373, 383 and 393 select header indices 0, 1,
  2 and contain little-endian 120 at bytes 4-5. Responses use command 84
  and echo the values. These appear to configure LED counts.
- Command 05 reports at frames 367, 369, 371, 381 and 391 have byte 8 = 1,
  zero colour count/data, and select all three headers and two onboard
  outputs. These appear to initialise lighting mode, but semantics remain
  unverified. Normal colour reports have byte 8 = 0.
- Startup also sends commands 21, 23 and 28 whose purpose is unknown.
  Command 10 produces configuration/version responses. Command 21 appears
  after setup and before colour updates; its meaning remains unverified.

`aura_initialised_replay.py` defaults to an offline dry run. It validates
eight exact captured setup reports (the existing 120-LED configuration),
then with `--send` sends those reports followed by previously validated
red/green/blue/red cycles. `Test-Aura-Initialised.ps1` temporarily pauses
LightingService, sends the already tested Lian Li lighting-sync feature
report, performs the ASUS test and restores LightingService in finally.
Fan Control stays running. Offline validation and PowerShell syntax check
passed. The user reported that absolutely no lights changed during this
first test, including the previously responsive motherboard logo. Windows
delivery succeeded. This is a regression in physical response and shows
that the replay of isolated setup commands did not establish a usable state.

V2 adds only exact captured frame 407 (`41 21 00 01`, zero-filled to 64
bytes), after the service's observed 2.2-second pause and before the first
colour update. Its hypothesised role is output activation; this is not
confirmed. It still excludes commands 10, 23 and 28. V2 result is pending.
The original test transcript is preserved; V2 uses timestamped logs.

V2 physical result: user confirmed only the motherboard logo changed.
`Aura-initialised-v2-20261011-174420-result.txt` confirms complete delivery
and service restoration. Adding frame 407 restored the logo's colour
response lost after the eight setup commands; this supports a role in
enabling/committing direct output, but does not establish exact semantics.
Neither ADD_GEN2_1's fan hub nor ADD_GEN2_2's GPU followed the colours.
Further identical colour replays are not justified. Remaining branches:
unobserved header configuration, ineffective physical ARGB path, or
endpoint sync behaviour. Direct vendor GPU control is the next independent
investigation path; no cable fault is proven and no new OpenRGB controller
has been built or validated yet.

User-confirmed wiring: Lian Li ARGB input connects to ADD_GEN2_1; KFA2 GPU
ARGB connects to ADD_GEN2_2. Both endpoints failed to follow Aura colours.
No splitter was reported. This does not demonstrate that either header's
physical data output works. The initial colour capture omitted service
startup; capture initialisation before changing the wiring.

- Whether the fan hub or GPU receives an effective ARGB signal. Neither
  followed Aura colours in earlier user tests; this is separate from USB
  command decoding.
- Initialization/configuration queries and arbitrary LED counts.
- A complete OpenRGB implementation and build validation.
