# Delta II RGB evidence checkpoint

Target: owner's USB wireless receiver, VID 0B05 / PID 1AFA. Research captures and vendor binaries remain outside the shipping diff. No new OpenRGB hardware writes have been enabled.

## Captured commands

All lighting requests below are 64-byte reports starting `CC 51`, with zero reserved bytes. Captured control transfer: SetReport, value `02CC`, interface 3.

| Key | Meaning | Data |
| --- | --- | --- |
| 10 | LED power | Byte 5: 0 Off, 1 On |
| 28 | Firmware effect | Byte 5: 1 Static, 2 Breathing, 3 Strobing, 4 Color cycle, 5 Rainbow; byte 6 brightness; bytes 7–9 RGB |
| 33 | Software mode | Byte 5: 0 disabled, 1 enabled |
| 30 | Software colour frame | Bytes 5–46: 14 RGB triplets |

Matching successful acknowledgements for keys 10, 28 and 33 echo `CC 51 key`, with remaining bytes zero. No frame-30 acknowledgement was observed. Queued replies for another key must not satisfy a pending command. `CC 22 33` is a software-mode notification, not an acknowledgement or an awake signal.

Capture identities:

- Built-in effects: `delta-lighting-20261024-001919.pcap`, SHA256 `3b65a8e06a9ea968a7cf900db631d23de11ce325a72f421e67519780f145c292`.
- Software mode: `delta-lighting-20261024-003540.pcap`; SHA256 retained in the software fixture. User confirmed purple first, then an animated rainbow wave while Aura Sync was still selected, before restoring the original effect. Recorded frame payloads include black and repeated purple. This does not establish a complete changing-frame animation trace, independent LED addressing, a safe update cadence or sleep safety.
- Final captured settings restore Breathing / purple (`5900A4`) / 50%, with matching acknowledgement.

## Installed native code

`AacAudioHal_x64.dll` SHA256 `f38f6a3af54ad86e60ddfdf9e7b3259d396337f4edbf0bed7c4ed9db3f490521` maps 1AFA to AacRH300WL. Its AacDeltaII class targets another PID. AacRH300WL software Apply uses key 30; mode selection uses key 33. Its 50 ms pause is an implementation observation, not validated headset throughput.

`AuraSdk_x64.dll` SHA256 `83709fec9e2210eb41d2be905211ae2bafedeec5d19c7f3f9b88d57186afec96` implements headset-type ownership (`40000`), but Enumerate activates HALs before filtering and the headset HAL opens its own receiver reader. Thus ordinary SDK enumeration does not yet meet the existing-owner requirement.

LightingService's SetLedMatrix path targets other device types. SetScript calls AllExecuterStop and starts its global sync engine, so it is also excluded from headset-only control. The Armoury HTTP framework checks related/trusted callers. Neither is an approved headset integration path. No ownership bypass or global Aura takeover is implemented.

## Remaining gates

- Prove a legitimate headset-specific control path through ASUS's existing receiver owner.
- Validate passive sleep/disconnect state and discard pending frames without wake-producing discovery.
- Establish physical software animation behaviour and bounded cadence.
- Share direct receiver ownership with existing manual battery reads and verify cancellation/serialization.

The pure encoder and capture-derived tests are complete; RGBController, backend integration and installation remain pending these gates.

## Resumed build and idle observation

The passive capture `delta-sleep-20261024-134549.pcap` (SHA256 `ec46c9ac114835e1702f81cd4777bdc31390716faffbd78a6c41a9112ba44590`) recorded no CC vendor reports. The owner observed the logos turn off after five minutes with the button still blue. This establishes the visible idle condition, not radio power-off or a usable HID sleep event. An active direct input subscription has not yet been tested across this transition.

Cached descriptors expose pages FF00, FF13, 000C and 000B; no standard LampArray collection is present among these four. Other collections are not used for lighting or audio commands.

Further static review excludes the inspected SetProfile candidate: it modifies legacy Aura ownership/global preferences, dispatches through Group/Mainboard handling and persists LastProfile. Targeted HAL activation has guarded enumeration; its internal factory must not bypass that gate. No runtime SDK probe or authentication bypass was attempted.

The full isolated custom core builds successfully after removing duplicate explicit controller entries; source discovery is automatic. SHA256: `fbbdb41c1d2c9c2f3009daba8d77b6a6bd5ba1826504131ff4a3727f3bb39b24`. MinGW reports a Qt metadata duplicate-section warning, but link exits successfully. The executable has not been launched or installed.

Receiver release invalidates in-flight transactions without waiting and closes retained handles after their bounded operation returns. The actual battery-code test confirms no charging query follows release. The pure frame queue covers closed startup, expiry, coalescing, deduplication and old-generation callbacks. Renewing permission after an unobserved expiry discards old frames. No validated hardware frame rate or awake provider is configured.

## Direct passive input subscription

`direct-sleep-20261024-161724` acquired the exact exclusive receiver collection, sent zero commands and recorded zero input reports during its native eight-minute cap. The logos were already off at the baseline (16:17:45 local time), with the button blue. The later observation was entered at17:47:42, after the recording ended at16:25:26. Consequently no awake-to-idle or wake transition was established, and sleep safety remains unvalidated.

Both services were restored Running with no reported restoration errors. The diagnostic helper previously waited for user input after its recorder had exited; its next version now stops waiting on recorder exit or deadline and restores services automatically. It also requires lit logos before and after ASUS closure, cancelling if a useful baseline is lost. Timed input/timeout tests pass in Windows PowerShell5.1. No additional live test has been run or authorized by the completed one-shot exception.
