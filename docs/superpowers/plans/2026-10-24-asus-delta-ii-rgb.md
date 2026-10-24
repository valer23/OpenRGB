# Delta II RGB Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Add native Delta II RGB control for firmware effects and animated OpenRGB effects through explicit ASUS integration and direct receiver backends.

**Architecture:** A headset RGBController delegates to one selected transport; direct transport shares exclusive receiver ownership with manual battery reads. A frame scheduler coalesces changes, bounds cadence and drops traffic whenever availability or transport safety is unproven. Backend capability and sleep behavior must be established from source/capture evidence before enabling the corresponding path.

**Tech Stack:** Existing C++17/Qt6/OpenRGB HID and dynamic settings infrastructure; native Windows COM for a proven ASUS SDK entry point; existing WindowsHidTransfer for bounded direct I/O. No new resident helper or bundled vendor DLL.

**Spec:** docs/designs/2026-10-24-asus-delta-ii-rgb.md (approved 2026-10-24)

## Global Constraints
- One shared RGB zone initially; no invented independent left/right addressing.
- Explicit ASUS integration / direct receiver selection; no automatic fallback.
- No service stopping, second USB reader, active battery fallback or wake-producing discovery.
- No audio/power/reset commands, global Aura ownership, or automatic standby writes.
- Preserve installed OpenRGB Custom, Fan Control, battery modes, motherboard Off and existing mouse behavior during research/trials.
- Keep diagnostic capture scripts and vendor artifacts in ignored research storage, outside the shipping source diff.

## Review Focus
- A device-matching error must not select a different ASUS headset or another receiver.
- A stale ASUS alive report must not authorize traffic to a sleeping headset.
- Concurrent battery refresh and RGB updates must have a single receiver owner.
- Disconnect/suspend/backend switch must discard queued colours rather than replay them later.
- A transport timeout must not leave an orphan request, unbounded worker or silent ownership fallback.

## Task 1: Establish control and sleep boundaries
**Files:** ignored RGB-investigation/delta-ii-rgb-research/{findings.md, SDK and capture artifacts}; docs/designs/2026-10-24-asus-delta-ii-rgb.md.
- [x] Verify feature branch inherits 8e31ec27 and preserves the installed configuration.
- [x] Verify G-Helper packet shape/model override and installed ASUS module identity; distinguish source observations from hardware proof.
- [x] Extract installed Aura SDK typelib without instantiating a hardware object; identify headset-scoped ownership candidate.
- [ ] Trace RequireTokenByType and device Apply/firmware effects to prove ownership scope and command semantics.
- [x] Passively capture awake user-driven headset lighting changes, saving HID/control packets only.
- [ ] Identify a volatile colour path and passive sleep/disconnect signal; capture a controlled transition if static evidence is insufficient.
- [ ] Record supported signatures, reply routing, cadence and ownership in the evidence ledger. Update subsequent task signatures only from this evidence.

## Task 2: Shared receiver ownership and protocol
**Files:** BatteryStatus/AsusDeltaIIBattery.cpp; new Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIReceiver_Windows.{h,cpp}; tests/asus-delta-ii/receiver_test.cpp.
**Boundary:** Move existing exact matching/identity, cancellation and exclusive handle logic behind one shared receiver owner; battery public entry points retain their current JSON contract and awake gates.
- [ ] Write failing transport tests for incorrect descriptors/identities, competing owner, concurrent manual battery/RGB requests, cancellation and unrelated replies.
- [ ] Observe the expected failures, then extract the real existing transport; do not duplicate it.
- [x] Implement only captured/verified RGB packet encoding and reply correlation (pure codec; transport integration remains pending).
- [ ] Run the existing complete battery suite and native mouse packet comparison with unchanged results.

## Task 3: Explicit ASUS backend and bounded RGB scheduling
**Files:** new AsusDeltaIIAsusService_Windows.{h,cpp} and AsusDeltaIIController.{h,cpp} beside the shared receiver; tests/asus-delta-ii/control_test.cpp.
**Boundary:** Select a backend explicitly. Send firmware effect changes on demand. Stream only through a proven volatile path while a validated state permits it.
- [ ] Write failing tests for exact device routing, headset-only ownership, unavailable ASUS integration, no backend fallback, stale awake indication, frame deduplication/coalescing, sleep/disconnect and timeout cleanup.
- [ ] Implement the proven ASUS API with device-type filtering and exact Delta II identity; no global SwitchMode/ReleaseControl.
- [ ] Implement the bounded scheduler and shared direct backend; cap cadence at the evidence-supported rate and keep only the newest frame.
- [ ] Test zero I/O at construction/detection/config edits and while state is unknown; ensure backend switch and teardown clear pending frames.

## Task 4: Native OpenRGB device integration
**Files:** new RGBController_AsusDeltaII.{h,cpp}, AsusDeltaIIControllerDetect_Windows.cpp; relevant existing controller build list; tests/asus-delta-ii/controller_test.cpp.
- [ ] Write failing integration checks for one shared zone; Off, Static, Breathing, Strobing, Color cycle, Rainbow and Direct capability flags; explicit transport setting and inactive-by-default acquisition.
- [ ] Implement through existing RGBController/settings mechanisms, with no construction-time firmware/audio queries or writes.
- [ ] Compile/link the normal custom build and validate actual plugin lifecycle, profile compatibility and all previous battery/mouse/shutdown regressions.

## Task 5: Isolated hardware validation and delivery
- [ ] Run an awake isolated trial: colour ordering, brightness, all built-in modes and Off, audio continuity, animation, measured update cadence and both backend paths.
- [ ] Confirm natural sleep/disconnect emits no RGB commands; confirm controlled resume and mode/backend changes do not replay stale frames.
- [ ] Run final whole-branch review, fix material findings, and rerun affected tests.
- [ ] Package through the existing Windows installer only after physical validation; preserve application identity, startup task, profiles and uninstaller.
- [ ] Verify installed battery readings, usual lighting, mouse sleep behavior and motherboard Off preference. Publish only to the user's fork when authorized.

## Execution ledger
2026-10-24: Tasks 1 research in progress. No RGB commands issued by OpenRGB and no service changes. Native REA MCP lacks Ghidra runtime configuration; existing Ghidra 12.2 can be used offline without changing MCP configuration. SDK3 metadata proves method existence/types, not side effects. Do not implement or enable a speculative SDK route merely because its name suggests isolation.

2026-10-24 capture checkpoint: firmware modes and Off confirmed in owner capture; Aura Sync capture confirms separate CC5133 mode and CC5130 14-triplet frame format. User observed initial purple then animated rainbow wave; user confirmed the wave began while Aura Sync was still selected, before restoration. Pure C++17 codec tests pass against captured bytes; no hardware path is enabled. Scope ownership static trace is complete but SDK Enumerate opens a new HAL reader, so it does not satisfy existing-owner integration. Service matrix APIs target other device classes. Passive CC2233 is software-mode status, not awake evidence. Research gate remains open for legitimate service route and sleep state. See docs/designs/2026-10-24-asus-delta-ii-evidence.md.

2026-10-24 receiver checkpoint: extracted the existing exact Windows collection/identity/descriptor checks and bounded WindowsHidTransfer into AsusDeltaIIReceiver_Windows.cpp. Core singleton provides serialized transactions and optional retained connection for RGB, borrowed by manual battery reads. Contention refuses immediately rather than opening/waiting for a second reader. All existing battery tests pass, including unchanged native mouse packet hash 4169482241693242891; new actual-battery-path fixture verifies queued RGB replies cannot satisfy battery replies and a retained RGB session is reused. No RGB hardware path is enabled; full core build is running.
Ruling: continue independently with shared receiver extraction while the ASUS service and passive sleep investigation proceeds — this work reuses proven existing battery transport and does not require speculative hardware commands. Task 1 remains open.
ASUS service research: SetScript invokes AllExecuterStop (006a0cb0) and starts the global Lua sync engine; exclude this API from headset-only control. No runtime call was made. The owner's configured idle timer is 5 minutes; passive sleep observation helper prepared with unchanged settings and a 30-second post-observation tail.

Pause requested by owner (going to bed): passive capture delta-sleep-20261024-010154 stopped early via stop marker. Baseline says purple breathing logos and blue 2.4 GHz button. No sleep transition was confirmed; do not treat elapsed time or partial capture as sleep validation. Cancelled marker records this explicitly. Full core build was interrupted at owner pause; targeted C++ compilation and complete battery suite passed, but full application build is NOT verified. Resume from this checkpoint, inspect the partial capture only as baseline evidence, finish full core compile and continue ASUS service/sleep research. Installed OpenRGB-Custom PID 28500 and FanControl PID 28620 were untouched.
Receiver tests: tests/asus-delta-ii/Run-Tests.ps1 passes all three executables (protocol, shared ownership, actual battery routing). Updated standalone battery probe linkage to include extracted native receiver. No hardware RGB integration or installation was performed.

2026-10-24 resumed checkpoint: complete core build succeeds (not launched/installed). Removed duplicate explicit controller source/header entries; OpenRGB discovers controllers automatically. Complete battery suite passes after non-blocking receiver release; native mouse packet hash remains 4169482241693242891. Four headset test executables pass. Reproduced/fixed queued-frame replay when permission renews after unobserved expiry. Commits 645f13f9 and 65014add contain tested teardown and frame-policy changes; no hardware RGB path is enabled.
Fresh passive idle capture confirms logos off/button blue, but no CC reports. Do not infer RF sleep. Metadata exposes no standard LampArray among the four headset HID collections. Service SetProfile and targeted private-HAL shortcuts remain excluded. A bounded direct read-only observer and service-restoration diagnostic wrapper are prepared in ignored research storage. Receiver availability check refused with Windows error32 and zero commands. Temporary ASUS stopping is awaiting the user's explicit diagnostic exception; no services have been stopped during this resumed run.

Direct passive observation 16:17: exclusive receiver ownership confirmed, commands_sent0, reports0 over8minutes. Baseline already logos-off/button-blue; later response was outside the interval, so sleep/wake validation remains open. One-shot service-stop exception used; do not repeat hardware/service actions without new approval. Research-only helper now has bounded observations, recorder-exit detection and lit-baseline gates, with four deterministic timing/input tests passing. Completed attempt restored both services; no RGB hardware commands or installed changes occurred.


2026-10-24 controlled delivery plan: completed shutdown support is pushed and PR10 targets the battery branch (PR9). The owner's new bounded awake RGB/service-closure exception is approved. Private awake-only diagnostic uses the existing shared receiver and captured codec, immutable per-stage control files, complete JSONL records, bounded cancellation, confirmed child exit and an independent recovery watchdog. Watchdog holds a separate recovery guard, uses a monotonic hard deadline and verifies exact diagnostic-child identity before forced cleanup if cooperative exit fails. Offline policy/input/JSONL/owned-child tests pass in PowerShell5.1 and7; actual watchdog readiness/completed lifecycle was tested without service actions. Four existing headset C++ tests pass. Independent review found no remaining material blocker for this bounded awake experiment.

Awake validation sequence: fresh lit baseline before AND after ASUS closure; red/green/blue; captured built-in effects and Off; brightness; short1Hz and5Hz software animation; explicit Breathing purple50% final setting. Every stage requires fresh awake confirmation and a physical/audio verdict. ESC, timeout, reader exit or failed transfer aborts without more RGB commands; original ASUS ownership is restored. Installed executable/configuration hashes are compared and FanControl/OpenRGB presence recorded. The helper has been launched; physical outcomes are PENDING. No new RGB support is installed or pushed.

Remaining independent gates: an asleep/visible-idle RGB experiment needs a separate possible-wake exception and cannot prove RF no-wake from dark LEDs alone. Ongoing streaming needs its own natural-idle validation. The legitimate ASUS existing-owner headset writer remains unproved; do not substitute SDK-created readers or a global Aura engine without an explicit design change. Full controller integration, both-backend validation, installer delivery and the headset PR remain incomplete.


Awake hardware result: all six native RGB stages pass physically, including both nominal1Hz/5Hz animations and restoration with audio normal. Native82writes all64bytes, no errors. Parent's final failure is an exit/output-flush reporting race, reproduced/tested and fixed offline; no repeat of passing hardware tests. Both services restoredRunning, installed files unchanged, FanControl/OpenRGB running. Sleep safety and ASUS existing-owner backend remain separate unresolved gates. See evidence document for exact report identity and cadence measurements.
