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


## Confirmed awake RGB output

The owner-approved awake trial `awake-rgb-20261024-221451` completed all six native stages with 82 writes of64bytes and no native errors. The owner confirmed red/green/blue, all captured firmware effects including Off, brightness, brief changing-colour animation at1Hz and5Hz, audio continuity, and final Breathing/purple5900A4/50% restoration. The final software-mode disable, LED-power enable and firmware-effect commands received matching acknowledgements. Native log SHA256: `3992eefa72bc561ba0cbf4c4c53dcf6c8ad430870774bf183b03529196c07fe4`.

The nine changing1Hz frames had logged write-completion intervals1001–1011ms (mean1004.875ms); the fifty changing5Hz frames had intervals197–218ms (mean204.694ms). Final purple frames are excluded. These short awake trials establish visible uniform-colour software control at the tested nominal rates, not independent physical LED addressing, extended stability or sleep safety.

The wrapper incorrectly reported final restore failure after observing process exit before reading the final output records. Native completion and all final acknowledgements are present; the owner also confirmed the last test passed. Offline tests now cover output flush/final reread. An earlier launch sent zero writes before a live-log sharing error; explicit shared-read handling is reproduced/tested with an actual live PowerShell writer. Both attempts restored services; installed executable/configuration hashes remained unchanged and Fan Control/OpenRGB continued running. These private harness fixes require no repeat of successful physical tests.

ASUS existing-owner integration and a reliable sleep policy remain open. No new RGBController has been installed or published.


## Passive manual power-switch report

Owner-labelled capture `delta-power-state-20261024-222803.pcap` (SHA256 `c6ba95efd994730d9928bc55010d84b4261c1021ed395215e27d1c30cf12ed69`) contains unsolicited64byte reports `CC1200000100` on manual headset OFF and `CC1200000101` on ON, followed by zero padding. The owner observed all lights off, then Breathing purple logos/blue button on. The installed audio HAL's input parser180045540 stores the corresponding state byte and dispatches its callback, agreeing with these observations. All vendor services, OpenRGB and Fan Control remained running; collector sent no commands.

The strict parser labels this LinkState, not awake state. One manual power cycle does not establish natural-idle behavior, RF sleep semantics or a renewable streaming permission. Tests reject unknown states, altered headers/reserved bytes, wrong sizes, null and unrelated reports. Connected alone does not authorize output. Natural-idle comparison remains pending.


## Natural idle and timer configuration

The next bounded, lit-baseline capture `delta-sleep-20261024-223508.pcap` (SHA256 `e7b9346dc2244452eb9f2a68c0d5550bb5d928a79399a132836b36e959988a24`) contains zero records. Owner confirmed lit at20:35:37UTC and sustained logos-off/button-blue at20:41:31UTC, followed by30seconds of observation. Services stayed running. This provides no passive natural-idle report; manual link-state notifications must not be promoted to an awake guard. It is consistent with local firmware behavior but does not establish RF power state.

Owner-controlled timer capture `delta-timer-20261024-224720.pcap` (SHA256 `e8985ec90276f644d22f976603b00ed016524b99833e82350c59cf9247511ee2`) identifies64byte setting reports `CC51370000` followed by minutes02/0A/05 at byte5 and unchanged bytes14/01 at6/7. Each has a matching allzero `CC5137` acknowledgement. Final five-minute write and owner's UI confirmation establish restoration; no separate timer readback was queried. These are setting packets, not sleep events. The production lighting API does not add timer writes.


## ASUS Aura Sync idle comparison

Owner-operated capture `delta-asus-sync-idle-20261024-225354.pcap` (SHA256 `b1aa70995f293770e8e81f97ae0f34eeec2df38cbbb1273639b44e0a7df74d97`) contains84URB records. The owner set two minutes, selected Aura Sync awake, reported sustained logos-off/button-blue, then woke the headset normally and restored Breathing5900A4/50% and five minutes. Both restored settings have matching acknowledgements. Services stayed running; recorder sent no commands.

Nine software frames occur during setup (black, then eight identical purple frames). The last is at20:54:53.733604UTC, before the20:54:58.9724017 lit baseline and176.222seconds before the idle observation. No frame follows idle before owner wake/restoration. This is not a continuous-stream sleep test. A battery notification arrived after the dark observation, so fresh battery activity cannot establish visible awake state either.

The owner separately authorized one possible-wake RGB-frame experiment with temporary ASUS closure and manual timer restoration. Its private harness has bounded setup/read-only waiting, a timed dark interval, fresh send confirmation, exactly one green frame and no retry/idle cleanup commands. Independent review and fake-I/O tests pass. Windows cancelled its UAC launch before any test session/device command; authorization remains unused and manual administrator launch is pending. No new RGB hardware path is installed or pushed.


## Single-frame idle test: visible wake failure

The explicitly authorized experiment `idle-rgb-20261024-232028` established solid purple while awake, stopped output until natural logos-off, timed30dark seconds, then sent exactly one green software frame after fresh owner confirmation. Native output contains four64byte writes total: three awake setup writes and one idle frame. It completed normally. The owner reported **woke**. Native JSONL SHA256: `04938186072da30a2c9deeb8efed97698c736c9806c7fc74026b5f8de55c5785`.

This fails visible idle preservation; unconditional software streaming is unsafe for the requested behavior. No reliable natural-idle transition was received. The one possible-wake exception is consumed. Both services were restored Running, no restoration errors, owner restored Breathingpurple50%/sleep5min, installed files unchanged, Fan Control/OpenRGB continued running. The reported service-stop API warning was recovered, not an end-of-test failure.

The owner confirms that normal PC playback alone wakes the logos. A private **Windows-only** Core Audio observer is prepared to correlate existing playback/pause, natural idle and normal audio wake without RGB output or audio stream creation. It matches the receiver's exact PnP container and rejects other speakers and the matching microphone before activation. Software peak metering is required; volume getter traffic remains to be measured in the paired USB capture. Shadow phases have structured pass/incomplete outcomes, and an audio-only deadline monitor cannot enter service-restoration code. No positive live shadow run or production awake permission is established. Other operating systems have no implementation or validation; the protocol remains platform-independent.


## Windows audio shadow observation (2026-10-25)

`delta-audio-shadow-20261025-003328` completed with the owner confirming playback,
natural logos-off/button-blue after pause, wake by normal PC audio alone, and
restoration to Breathing purple/50% and five minutes. No ASUS service was stopped.
The collector created no playback/recording stream or HID handle. Saved audio-log
SHA256: `bb9d22a18ae715b1f2ddbc0bb6e10fb495982498339e34e9f30b4c8f34983c18`;
HID/control capture SHA256:
`ccb2a21d5a2841a1dd24c44126d844c7f603326c99a401538e3c505ebc45f7da`.

All 2,460 samples were valid, with polling gaps 106–125 ms. Confirmed playback had
275/275 activity-positive samples; confirmed pause through dark had 0/1,582;
confirmed audio wake had 46/46. The exact receiver-container render endpoint used
a software peak meter (hardware flags 3, meter-support bit 4 absent). The capture
contains only eight URB records: the owner's two-minute/five-minute timer writes
and their acknowledgements. No additional HID/control traffic was recorded during
observer activation or polling in this configuration; audio payloads were excluded.

This qualifies a research candidate, not an RGB sleep-safety guarantee. Human
instruction/typing timestamps cannot establish the exact audio-start-to-logo-wake
latency. Mute, exclusive playback, device switching and replug remain untested.
The source is Windows-only and is not installed or enabled by default.

The platform-neutral audio gate requires an explicitly armed, qualified source,
continuous fresh non-muted receiver-matched playback, and a settling interval.
Invalid samples immediately revoke frame permission and drop pending colours;
old/duplicate observations cannot extend a lease. Five focused test executables
pass, including actual gate-to-queue revocation before an existing lease expires.
The experimental three-second settling/250 ms freshness values are test policy
choices, not physically validated wake timings. No live RGB path uses them yet.
