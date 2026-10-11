# Lian Li SL-Infinity sync investigation

Hub: VID 0CF2, PID A102, firmware display v1.4. USBPcap1 address 12 on
2026-10-11 (must rediscover after USB topology changes).

## Confirmed startup behaviour

`Fan-startup-20261011-161523.pcap`: 210 packets over 31.551 seconds.
Frame 7 contains the ARGB plugin's motherboard-sync report:

```
E0 10 61 01 00 00 00 ...
```

The 353-byte HID output report uses SET_REPORT, wValue 02E0, interface 1.
Its completion (frame 8) reports USBD_STATUS_SUCCESS. This demonstrates
transport delivery, not that the firmware accepted the intended mode.

Next reports disable motherboard RPM sync independently for each channel:
`E0 10 62 10`, `E0 10 62 20`, `E0 10 62 40`, `E0 10 62 60`.
Regular fan-speed updates follow. No other lighting command was identified
in this startup capture.

The installed ARGB DLL constructor sets enableARGB=true; the regular DLL
does not. Their SHA256 hashes differ. Installed plugin configuration has no
disabled plugins. The regular DLL is saved as
`FanControl.LianLiPlugin.dll.before-ARGB-test`; ARGB DLL is currently active.

The user reports that the fans remained on rainbow during the direct
motherboard colour replay. This does not prove an ARGB wiring fault. Compare
the vendor application's applied sync command before changing cables.

Source: EightB1ts/FanControl.LianLi, src/LianLiPlugin/LianLi/Devices.cs,
SetARGB() SLI case. Local reference clone: FanControl-LianLi-investigation.

## Vendor comparison

`LConnect-sync-20261011-162335.pcap`: 518 packets over 34.454 seconds.
Frame 349 sends `E0 10 61 01 00 00 00`, but as a **feature report**:
wValue **03E0**, interface 1, length **7**. Frame 350 completes successfully.
The plugin instead uses wValue **02E0**, length **353** for the same prefix.
Read-only Windows HID capabilities confirm input=65, output=353, feature=7.

The user observed that enabling vendor sync froze the rainbow animation,
leaving fixed colours. This is a different response from the plugin, but
does not yet prove that the motherboard ARGB signal reaches the fan LEDs.
The vendor test restored the regular plugin and stopped/disabled L-Connect.
Next: replay the exact seven-byte feature report, then motherboard colours.

## Exact feature replay result

2026-10-11: `Exact-hub-sync-result.txt` confirms successful delivery of the
seven-byte feature report, followed by all four motherboard colour cycles.
The user observed that only the motherboard ROG logo changed. Consequently
the plugin report-type mismatch alone does not explain the missing colour
response. The motherboard's physical ARGB output and the path to the hub
are not yet verified. L-Connect remains stopped/disabled; original regular
Fan Control plugin is active. LightingService was restored to running.

Do not infer a cable fault or conclude the headers work from successful USB
reports. Establish which headers and any splitter/controller are involved
before a physical signal test.

## Direct lighting transport comparison (2026-10-11)

The same vendor capture contains direct lighting commands, not only sync.
Tshark verbose inspection confirmed frame 23 (E0 10 60 01 03 00 00) and
frame 33 (E0 17 01 00 00 00 00) are seven-byte feature reports, wValue
03E0. Frame 31 is a 353-byte colour output report, wValue 02E0.
All eight output colour channels E0 30 through E0 37 and corresponding
static feature commits E0 10 through E0 17 are present in this capture.
Colour payload order is RBG, matching the existing OpenRGB driver.

The existing OpenRGB SL-Infinity driver used hid_write with 65-byte
buffers for the start and effect commits. The experimental patch uses
hid_send_feature_report with seven-byte buffers, preserves output
colour reports, and uses the configured fan count instead of hardcoded 4.
Unused zones are skipped rather than preventing later populated zones
from updating. Hardware-free tests compile the actual low-level driver,
verify report types, fan counts, RBG payloads, zero padding and no writes
for empty channels.

The direct Python test sends only port LED-count setup, colour output
and static effect feature commits. It excludes PWM, RPM-sync, firmware,
merge and motherboard-sync commands. It leaves the final requested
colour purple. Fan Control remains running. Hub capabilities were
independently confirmed read-only; physical colour response is pending.
Earlier vendor settings recorded port fan counts 3,2,1,0; the launcher
requires confirmation of actual port counts before writing.
