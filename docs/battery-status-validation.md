# Battery Status software validation - 19 October 2026

Status: software built and reviewed; owner-hardware validation pending. Neither device has a validated live battery reading yet.

- Feature branch: codex/battery-status, isolated managed worktree, base cb1385fa.
- Built core product-source revision: 5bbeecd23f6bd5cc58029697f8a4d35ce524bcab (subsequent tests/documentation changes do not alter the product code).
- Full Windows Qt 6.8.3 / MinGW 13.1 core build: exit 0. Existing installed-build MinGW portability shims generated locally, no vendored source edits.
- Plugin DLL build: exit 0.
- Core version-only startup smoke: exit 0, before hardware initialization.
- Full software suite: cache/subscription/freshness/invalid values; tray presentation and alert thresholds; actual plugin DLL old/new host RTTI and lifecycle; installed effects DLL metadata/interface load/unload; actual Corsair controller with fake HID; strict parsers and disabled ASUS gate; both reproduced review regressions all pass.
- Both review findings fixed: no active Corsair battery GETs remain, eliminating late GET0F/GET40 mixing and the associated disconnect-state overwrite. Passive receiver notifications need separate owner validation.
- One pre-existing Qt/MinGW duplicate metatype-section warning remains, matching prior installed build logs. No new compilation error.
- Isolated trial prepared under build-battery/trial. All 11 profiles are byte-identical to the installation; non-battery settings compare equal. Both hardware validation gates remain closed. No trial was launched against hardware.
- Installed executable remains SHA256 1080F2E6CEEC2232C733786F458DD4C825248006ED5B9A49C60E13344BE41888.
- Trial executable SHA256 2DBAFE79496CE53E1D60A60695257CD917947EF5489950A636BC4EC46F222464.
- Battery plugin SHA256 8A648D52B90DC0779285816FEB427DA0287B3D22C412EA25E7A9D8DDDC4BE18F.
- Installed OpenRGB and Fan Control remained running. No installation, service stop, startup change, public post, PR/MR or push occurred.

## Important limit

A successful awake Delta II read would validate framing and the reported value only. It would not establish no-wake background polling. A Corsair notification parser passing fake-HID tests does not establish the owner's receiver emits that frame. The UI will correctly show unknown/unavailable until evidence is collected.

The next step is the pending owner-confirmed awake headset read. A later isolated full-app mouse trial requires the owner to exit normal OpenRGB through its tray so there is one receiver owner; Fan Control stays running. Never start a second hardware-detecting OpenRGB instance. The trial launcher enforces this guard. Native 15-minute firmware sleep remains pending; no repeated long wait is required for this software stage.

See plugins/BatteryStatus/README.md and the trial/probe scripts for the concrete commands and safety guards.

## Awake headset attempt - 19 October 2026, 08:05 local

The owner confirmed the Delta II was awake and authorized one battery/charging read. The exact vendor collection was found, but Windows refused exclusive access. No battery or charging command was sent, and no value was obtained. A subsequent metadata-only check confirmed Windows error 32 (sharing violation), with commands_sent=0. No shared reader was opened and no automatic retry or polling was enabled.

Armoury Crate, ArmouryAudioAgent, ArmouryCrateService and LightingService were running. This is consistent with an ASUS reader owning the collection, but the owning process has not been identified. FanControl and installed OpenRGB remained running. No vendor component was stopped. The approved design requires reporting busy access rather than stopping a vendor service, so any temporary service stop needs an explicit exception from the owner.

Evidence is retained under build-battery/probe/20261019-080512-589.json and 20261019-080610-785.json. An Inspect probe mode now reports exclusive-interface availability and the Windows error without sending device commands.

## Authorized service-release attempt - 19 October 2026

The owner explicitly authorized stopping and restoring ArmouryCrateService. The execution token is not an administrator token. The RunAs launcher did not produce a visible approval prompt, and the service remained Running. The pending elevated helper was replaced with an unconditional cancellation script before any service change, so a late launch cannot stop the service or query the headset.

After the owner closed the headset's Armoury component manually, a metadata-only check still returned Windows error 32. ArmouryAudioAgent (PID 14340) and ArmouryCrateService remained running; this does not identify which process holds the interface. A direct authorized service-control attempt was denied by Windows before Stop succeeded (stopRequested=false, probeAttempted=false, service still Running). No headset battery or charging commands were sent. FanControl, installed OpenRGB and LightingService remained running.

Further exclusive acquisition requires the owner to release the remaining ASUS reader using their Windows session, or provide a visible administrator terminal for the already-authorized transactional stop/read/restore. Do not open shared access as a workaround.

## Administrator service test and precise owner lookup

The owner's administrator-terminal transaction stopped ArmouryCrateService, attempted exclusive access once, and restored the service successfully (restoreError=null, after=Running). The interface still returned Windows error 32 while that service was stopped. This does not identify the remaining owner and does not prove that ArmouryCrateService had no handle; multiple owners may exist. No battery/charging command was sent.

Cached Windows metadata confirms the exact collection is the expected vendor interface: MI03/COL04, usage page FF00, usage 1, 64-byte input and output reports, no feature reports. A diagnostic helper obtained this using zero-data-access metadata handles only (no input/output/feature report operations). The Windows instance is HID/VID_0B05&PID_1AFA&MI_03&COL04/8&2CBE7DFC&0&0003; cached PDO name at inspection was \Device\0000008b. Evidence: build-battery/probe/headset-metadata.json.

Prepared scripts/Find-DeltaII-Owner.ps1 for a read-only exact-object handle lookup in the owner's existing administrator terminal. It dynamically resolves the PDO, validates the downloaded Microsoft Handle executable's Authenticode signature, and passes only listing/search options; it does not stop services/processes, close external handles or issue USB commands. Handle was downloaded from Microsoft's official Sysinternals site; signer Microsoft Corporation, signature Valid, SHA256 24BAFCC570CC9BBB6B6E6652A57A519E0464E3996891AABA6F55299CCE20B04F. Its first-run license dialog is left for the owner to review. No further service closures are proposed until the lookup identifies the actual remaining owner.

Sources: https://learn.microsoft.com/en-us/sysinternals/downloads/handle and https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/opening-hid-collections.
