# ASUS ROG Delta II RGB support

Date: 2026-10-24
Branch: codex/asus-delta-ii-rgb
Base: 8e31ec27 (verified shutdown and battery fixes)
Status: approved by owner on 2026-10-24; implementation investigation in progress.

## User outcome

Expose the wireless Delta II side logos as a native OpenRGB headset, usable with both firmware effects and the existing Effects plugin. Support the user's ASUS-running setup and systems without Armoury Crate. Preserve audio, battery monitoring, Fan Control, motherboard shutdown preferences, and mouse sleep/lighting behavior.

## Device and modes

Target only receiver VID 0B05 / PID 1AFA and its validated vendor collection. Start with one shared RGB zone representing both side logos: source evidence establishes one RGB colour, not independent left/right addressing. Do not advertise separate outputs without a capture proving them.

Expose Off, Static, Breathing, Strobing, Color cycle and Rainbow, with supported brightness controls. Expose software-controlled colour updates for Effects only after confirming a volatile update path and a safe bounded cadence. Coalesce identical colours and keep only the newest pending frame. Do not present repeated persistent-settings writes as a streaming implementation.

## Explicit transport choice

- ASUS integration: communicate through an established ASUS API that uses its existing receiver owner. Never open the receiver from this backend. Verify supported caller/authentication rules and exact device routing. Do not spoof trusted applications or bypass authorization.
- Direct receiver: one exclusive receiver session, refusing competing owners. Reuse/refactor the battery transport for exact identity, report validation, bounded I/O and cancellation. Serialize manual battery requests and RGB traffic through the same owner; never run two readers. No service stopping or automatic ownership changes.

No fallback between backends. ASUS absent/busy/unsupported and receiver busy must remain explicit errors. Battery source selection remains independent: stale or missing ASUS cached battery data never initiates a USB query.

## Sleep and lifecycle

Metadata discovery alone sends no device commands. Detection, construction and editing settings must not initialize lighting or change audio/power settings. Do not use battery queries, connection keepalives or lighting traffic to discover whether a headset is asleep.

Animated updates require a reliable, validated indication that sending is safe while the wireless child is awake. On asleep/disconnected/unknown state, suspend writes and discard pending frames. Resume only from a validated passive condition or an explicit user action with an awake headset. A recent cached battery value is not proof of wakefulness. The source of a sufficiently reliable state signal is an investigation gate, not an established feature.

Timeout, suspend, disconnect and unload cancel/join work and release ownership without resetting the headset or sending a final wake-producing command. Restore no guessed state. No changes to equalizer, microphone, sleep timer, audio processing, ASUS services or Fan Control.

## Evidence and investigation gates

G-Helper's DeltaII model has no lighting overrides; its AsusHeadset base supplies CC / 51 / 28 colour/mode/brightness and CC / 51 / 10 on/off. These are protocol leads rather than owner-device validation. Installed ASUS View/6906/service.js (SHA256 EC2433623FAA9CA8D3E1226B2C3038302156F4C128741A436DD21243057B3AA9) advertises Aura Sync and a lighting route with a preview flag. Neither fact alone proves a usable external streaming API.

Before implementation is enabled on hardware, establish:
1. A legitimate ASUS integration entry point and exact Delta II routing, supporting both firmware mode commands and volatile colours.
2. Direct command framing/acknowledgements and whether continuous colour changes are volatile rather than persistent writes.
3. Safe update cadence and passive sleep/disconnect behavior for each backend.

If a gate fails, report the concrete missing capability and continue research; do not quietly remove animated support or substitute built-in effects for the requested scope.

## Validation and delivery

Offline tests must cover exact device matching, packet encoding, reply correlation, bounded cancellation, queued-frame coalescing, explicit backend selection, no fallback, shared battery ownership, no writes at detection, and zero output while sleep/availability is unknown. Preserve existing complete battery tests and native mouse packet comparison.

Use an isolated hardware trial while the owner confirms the headset awake. Check known colours, all firmware modes, brightness and Off, then animated effects and audio stability. Observe natural sleep and disconnect/resume. Verify both ASUS-running and ASUS-absent configurations with no competing reader. Preserve and verify motherboard Off preferences. Only then update the installed OpenRGB Custom using the existing Windows installer/uninstaller and startup/profile setup. Keep diagnostic captures and trials out of the source diff.

References:
- https://github.com/seerge/g-helper/blob/main/app/Peripherals/Headset/Models/DeltaII.cs
- https://github.com/seerge/g-helper/blob/main/app/Peripherals/Headset/AsusHeadset.cs
