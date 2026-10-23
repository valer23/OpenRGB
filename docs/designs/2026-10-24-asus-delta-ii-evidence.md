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
