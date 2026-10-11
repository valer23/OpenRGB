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
The user confirmed one hub with port fan counts 3,2,1,0. The launcher
uses these confirmed counts.

## Direct test failed; vendor sync-off implementation verified

The user observed no fan lighting changes during direct test
`LianLi-direct-20261011-191707-result.txt`, despite successful API delivery.
This rejects feature transport alone as a sufficient fix. The earlier exact
sync replay enabled motherboard lighting sync, and the direct test did not
disable it. Current mode was not queried, so retained sync is a hypothesis.

Read-only ILSpy inspection of installed L-Connect-Service.exe located
`LConnectService.Products.Ene6K77Fan.SLInfinityController`. Its
`ApplyMotherboardARGBSync(false)` disables sync, then reapplies lighting
configuration. It also suppresses direct colour updates while sync is on.
Inspection of installed L-Connect.Core.dll, class
`LConnectCore.Products.Ene6K77Fan.SLInfinityDevice`, independently verified
that `SetLightingMotherboardSync(false)` uses feature command E0 10 61 00
00 00 (padded to the known seven-byte feature length). It preserves the
separate PWM/RPM sync command 62; that command is not part of this test.

L-Connect.Core.dll SHA256:
4D11F179841FD9877B0CC79FA85E446625367A4EA91327EDC0C0F3D6ED9EC600
ILSpy command version: 11.1.0.9782. Vendor code was inspected as data;
no vendor device constructor or hardware method was executed. Decompiled
vendor files and downloaded analysis tools are ignored by Git.

The revised launcher uses `--disable-sync` before the same direct colour
sequence. This command is verified from the vendor implementation, not an
exact sync-off packet in the earlier capture (which only contains sync ON).
Physical response to the revised test was subsequently confirmed; see below.

## Physical direct-control success and native integration

The user confirmed red / green / blue / purple on every fan after the revised
sync-off test; fans finished purple. Channels updated sequentially, with a
user-estimated delay of less than half a second between physical ports.
The prototype intentionally waits 30 ms between each of the five lighting
reports per populated port. Each port has centre and side colour channels;
this accounts for staggered application and is not a failed colour update.

Native integration disables motherboard lighting sync lazily before the
first lighting start report, checks successful delivery, and aborts colour
writes on failed sync/setup. Detection does not send lighting commands.
The native driver uses its existing 5 ms inter-report pause instead of the
prototype's 30 ms. Colour buffers are zero-initialized, and Custom-mode
single-LED callbacks now apply colour changes rather than returning via an
inactive effect-mode callback. Other effect mappings remain unverified.

`Build-OpenRGB-KFA2.ps1 -IncludeFans` enables an isolated combined build.
Its local fan detector preconfigures the confirmed 3,2,1,0 counts: zones
48,48,32,32,16,16,0,0, paired centres/sides. This preset applies only to the
local test detector; normal discovery remains configurable. The isolated
build exposes only Static for fans, retaining the two proven GPU modes.
It uses separate settings and does not include motherboard, RAM or keyboard
detectors. Native GUI fan colour verification was subsequently confirmed; see below.

The combined native application linked and deployed successfully on
2026-10-11. Read-only discovery with Fan Control running registered both
`Lian Li SL Infinity (local static test)` and the KFA2 GPU, and exited 0.
See test log `OpenRGB_20261011_194027.log`. Non-admin PawnIO initialization
warnings relate to unused system SMBus interfaces; both selected lighting
detectors registered. Native HID boundary checks also passed after the
final driver edit. The user subsequently confirmed that fan colour control works in the native GUI.

## Native GUI physical confirmation

On 2026-10-11, the user reported that the combined OpenRGB build works
and supplied a screenshot showing the local SL-Infinity controller selected
in Static mode, with purple colour 4E00AD. This establishes physical fan
colour control through the native OpenRGB interface, in addition to the
earlier independently verified GPU control. Dynamic effects and rainbow
remain untested in this build, which currently exposes Static only for fans.

## Independent hardware animation trial

The local detector now exposes Static, Breathing, Spectrum Cycle, and Rainbow
Wave. Vendor SLInfinityController maps independent inner and outer effects
respectively to the same codes 2, 4, and 5. These are distinct from combined
Rainbow (50) and combined RainbowMorph (63); combined and merged effects
remain excluded. The existing speed/brightness encoding is retained.
Breathing starts with purple on all six colour slots.

Colour and zone callbacks now reapply a selected hardware effect rather than
sending the per-LED Static commit, which would stop the animation. Native
boundary checks cover all six populated centre/side channels, effect codes,
fan counts, feature/output report lengths and default speed/brightness.
Physical animation, speed, brightness, and return-to-Static confirmation are
pending. No cooling commands are added.

The native HID boundary checks passed and the combined GUI executable rebuilt
successfully on 2026-10-11. Physical animation confirmation remains pending.

## Physical animation confirmation

The user confirmed all exposed effects work: Rainbow Wave, Spectrum Cycle,
Breathing, and returning to Static. This confirmation covers the isolated
local GUI build on the six-fan hub with port counts 3,2,1,0.

The combined application is now named OpenRGB-Custom. Existing build and
settings directories retain their paths so saved profiles remain available.
