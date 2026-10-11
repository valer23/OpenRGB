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
