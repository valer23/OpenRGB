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
