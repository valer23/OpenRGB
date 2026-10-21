# Battery Status software validation - 19 October 2026

Status: two-device feature remains in progress. The Corsair passive battery path is validated on the owner receiver and confirmed visible in the isolated tray trial. Delta II awake framing is validated, but sleep-safe acquisition is unproven and automatic headset queries remain disabled.

- Branch: codex/battery-status, base cb1385facc3337b0db7f9956624dbc111a80be38, in an isolated worktree.
- Completed hardware trial (the owner has since returned to installed OpenRGB): core product revision 3957284d; plugin metadata fix b8262ce8. The owner confirms the Battery Status menu, mouse percentage and usual lighting effects work.
- Captured passive mouse reports decode to 63%,62%,61%,59% and100%. The continuing trial log records native handoff at239536ms, resume on input at261325ms, another handoff391707ms and resume2506705ms. They arrive through the existing matched receiver event reader; no battery GET or additional reader was added.
- Delta II awake exchange returned 89%, charging=false. A later one-query sleep attempt returned100%, but the owner could not establish pre-test sleep state or whether it woke; that experiment is inconclusive. Receiver presence and silence on the desk do not establish sleep or wake state.
- Full software suite passes: cache and presentation, DLL lifecycle and cross-DLL RTTI, API-v5 Effects compatibility, actual Corsair driver with fake HID, native idle and delayed-response regressions, captured protocols, acquisition gates, simulated service restoration, and trial packaging.
- The ASUS scheduler is now connected to host subscriptions and an exclusive in-process reader, behind the default-closed ReceiverLevelValidated hardware gate. Shared-token lifecycle and bounded Windows cancellation pass hardware-free tests. A receiver response is labelled receiver-reachable, with headset sleep state unknown.
- Newly built core product revision7ac0b285 passes full build and an explicit version-only process startup (exit0). It is prepared separately and has not replaced the live trial. SHA256 276F2F897E4969C8A95B8F409C582118F42AD85AA564A7C62E18C39252C764A7.
- Trial executable SHA256: 980BFC431BF3127DDDF900763E3401BEEEA6FD529D6DB445CC9F3713EAEA3F60.
- Previous trial battery DLL SHA256: 3A03C3A98A56AC5A64F85854099C118339C194578533467A461026E01A9597FC. Subsequent presentation changes are built separately and have not replaced the live DLL.
- Installed executable SHA256 remains 1080F2E6CEEC2232C733786F458DD4C825248006ED5B9A49C60E13344BE41888. Installed profiles and files are unchanged. Fan Control remains running.
- Both ASUS services were restored to Running after the completed14:21 experiment. Runtime acquisition must report busy access rather than stop vendor components.
- The Intel Skylake PawnIO failures and Creative AE-5 warning also occur in the installed baseline. Both AMD PIIX4 SMBus interfaces initialize successfully in the administrator trial. The trial-specific missing Effects migration folder is fixed in packaging and covered by a regression; the isolated folder awaits its next update.
- No PR or push has occurred. The owner explicitly authorized a PR in the public fork valer23/OpenRGB; no upstream publication is authorized.

## Remaining hardware evidence

The owner has authorized a controlled follow-up query while naturally asleep, with an observed-wake answer before ASUS components are restored. The observed-baseline procedure is scripts/Invoke-DeltaII-SleepTest.ps1 -WaitForAutoPowerOff -AuthorizeOneBatteryQuery -TemporarilyCloseAsusReaders. It releases readers, waits for the owner to type off after BOTH side-logo lighting and the blue2.4GHz button light turn off naturally, then asks whether either light returns after its one query. Any other baseline answer cancels without querying. An existing pending LightingService transition is allowed up to10s to settle before a fresh ownership scan; a timeout refuses the test. It preserves initial ASUS service states, closes only identity-checked ASUS reader launchers, invokes one CC1207 query, and requests launcher restoration. It never stops OpenRGB or Fan Control. Charging is deliberately not queried in this sleep experiment.

A reply alone cannot prove no-wake. If the experiment wakes the headset or remains inconclusive, automatic active requests stay disabled until a safe source is demonstrated. Normal audio continuity, headset disconnect/reconnect and charging transitions remain hardware validation items. Mouse native handoff and input resumption are logged; independent visual input-latency measurement is not claimed. Do not repeat a long firmware-sleep wait solely to rerun existing software tests.


ASUS's Q23401a guide lists an unlit2.4GHz button as off or fully charged; that LED alone is not sleep evidence. See the [official quick-start guide](https://dlcdnets.asus.com/pub/ASUS/Accessory/Headset/ROG_Delta_II_Wireless_Gaming_Headset/Q23401a_A701_ROG_DELTA_II_QSG_WEB.pdf?model=ROG+Delta+II+Wireless+Gaming+Headset). The owner reports unplugged charging USB and illuminated side logos/blue2.4GHz LED, providing visible markers for a controlled natural transition. No-wake remains unvalidated until an actual before/after observation succeeds.

The fresh whole-branch review found one Important lost-notification race. A deterministic actual-source predicate/wait interleaving reproduced a full-interval unload stall; guarding the stop transition with the wait mutex fixes it and the full suite passes. The service now also uses existing OpenRGB system suspend/resume handlers to stop the headset worker regardless of suspend-profile settings, retain samples as last-known and prevent new subscribers starting I/O while suspended. Resume schedules a fresh exact-identity/descriptor/exclusive-open check before a new level request; disconnect/reconnect uses the same per-attempt revalidation. Receiver presence is never treated as awake evidence. Compiled acquisition gate remains closed.
## Reverse-engineering tooling

REA 6.7.0 is installed and its Codex stdio MCP registration and 207-tool handshake are verified. Its skill is installed; the active chat needs a reconnect to expose newly registered MCP tools. The CLI is usable. No native disassembler engine is installed. A static analysis of the installed ASUS model6906 service.js completed with a partial/truncated semantic graph; it does not establish a sleep-safe battery source. No ASUS IPC request, code injection or installed-artifact edit was made.

The following dated sections are historical attempts; the current state above supersedes their pending instructions.

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

## Successful awake Delta II read - 19 October 2026, 09:26 local

After owner-initiated ASUS framework closures, both exact-PDO owner scans reported no matching handles. LightingService was already Stopped, and the transaction preserved that state. The bounded exclusive probe exited 0 and decoded receiver reports as percentage=89, charging=false. It sent the two authorized battery/charging requests only; firmware was not queried. No independent display/gauge comparison was supplied, and charging=true remains untested.

Captured prefixes (12 bytes each; full reports were not retained):

- Level: CC 12 07 00 00 05 59 14 01 00 00 00; byte6 is 0x59 (89).
- Charging: CC 12 08 00 00 00 00 00 00 00 00 00; byte5 is 0 (not charging).

Evidence: build-battery/probe/lighting-trial-20261019-092603-471.json and a sanitized, committed copy in tests/battery-status/fixtures/delta-ii-awake-20261019.json. The protocol test replays the real prefixes and rejects interpreting the charging report as level or vice versa. Diagnostic regression tests cover Handle's successful no-match exit1, failed scans, and Running/Stopped service state preservation on both successful and failed probe launches.

This validates one awake exchange through the owner's receiver. It does not validate queries against a sleeping headset, passive notification delivery, audio continuity, hotplug, charging transitions, battery accuracy, or automatic polling. No background acquisition was enabled and no standalone sample was inserted into the core as current telemetry. Normal ASUS components must still be able to retain their existing ownership; runtime service/process termination is not a plugin strategy.

At the end of the owner trial, both LightingService and ArmouryCrateService were manually stopped. The owner should restore these originally Running services and reopen Armoury after testing. Fan Control and installed OpenRGB remain running.

## Integrated manual tray trial — 21 October 2026

Core source f51d2475 with plugin SHA2566939E8CB27419EAD65AAAF269D6ECE4D37BAA26987E5F478276AB5DB27CE0B91 ran as isolated OpenRGB-Battery-Trial11756. Installed1.0.20261020.1 files and11profiles were preserved; FanControl28840 stayed running.

The owner saw Corsair77%. The trial log records passive frame01010F000203 at1664171ms, decoding LE16(0x0302)/10 to77, with no added battery GET. Native firmware handoff/software restoration were also logged.

Headset refresh initially returned Windows error32 and opened no competing reader. Even after framework root32136 and LightingService were stopped for the approved diagnostic window, the ASUS launcher restarted a new framework reader10208 under root41352 and reclaimed the exact PDO. This was confirmed by an elevated read-only handle scan, not inferred from process presence. The initial helper was restored without errors.

The corrected diagnostic included the verified ArmourySocketServer launcher15964. LightingService was temporarily stopped fromRunning; ArmouryCrateService was alreadyStopped and stayedStopped. Two owner scans separated by5s were clear before the owner used the actual tray confirmation while the headset was awake during gameplay. The owner reported84% last known, then explicitly confirmed “manual reading, not charging at last refresh.” They also confirmed audio and usual lighting continued normally. This is owner-observed UI evidence, not a retained84% USB packet capture.

The release signal restored LightingService toRunning and requested the ASUS launcher relaunch, with no restoration errors. The diagnostic helper sent0USB commands; the core-owned tray action performed the authorized awake acquisition. Automatic ASUS polling remains disabled. Keeping ASUS readers running can still prevent future manual refreshes; retained data remains last-known, with age. No automatic vendor stop or sleep-safety claim is made.

Remaining qualifications: charging transitions, independent battery accuracy, physical hotplug/system suspend tests and other platforms. These limits are stated in the fork PR; the strict automatic headset acquisition design is not claimed completed.

## 21 October: passive ASUS cache full-app trial

Built cache-provider revision0c0870cf after the hardware-free suite passed. Replaced only the exited isolated trial core/plugin, retaining rollback binaries and verifying all11 profile hashes. The elevated trial launched as PID45408; ASUS Framework, LightingService and Fan Control PID28840 remained running. The startup log loaded the usual Halloween - The Dark profile/effect. The owner confirmed ASUS percentage and ASUS cached report visible, mouse battery working, and headset audio/usual lighting working.

This verifies the current installed ASUS log format and tray integration. ASUS supplies the measurements; OpenRGB adds no headset USB reader or requests. Existing ASUS logging remains a dependency. Cached report age is not proof of physical measurement time or headset sleep state. Automatic headset hardware queries remain disabled. The installed OpenRGB-Custom executable was not replaced.
