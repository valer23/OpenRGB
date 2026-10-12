# Custom lighting effects

The local OpenRGB Effects Plugin is based on official source commit
`8288ef45eb27d98a237b11ae21fa76be17978c2a` (plugin API 5), built with the same
Qt 6.8.3 / MinGW 13.1 runtime as the custom OpenRGB application.

## Profiles

- **Rainbow Wave** uses the plugin's existing effect, shared timing across all
  assigned zones, with the current profiles using 30 renderer FPS. Multi-LED zones show a wave; the GPU
  and motherboard logo each cycle as one colour zone.
- **Brimstone** uses an added native effect. Its dark-orange background,
  orange/red rain palette and moving waves are derived from the supplied iCUE
  XML. Original profile: Brimstone by Lewis Gerschwitz, exporter 5.9.105.
- **ValWhite** remains the sign-in default. Loading it stops the effects.

This is a visual adaptation, not an exact iCUE rendering engine. iCUE's rain
random generator, speed-enum meanings, blend details and Corsair-specific LED
coordinates are not fully defined by the profile. Exported 1.4/1.2-second
keyboard waves are mapped onto normalized OpenRGB zones. Other exported device
variants are retained in the analysis report rather than silently merged into
one physical device.

## What was decoded

`extract_icue.py` resolves serialized lighting type identifiers and records
enabled named Brimstone layer definitions, ARGB colours, transitions, durations,
speed, direction and wave parameters. The JSON includes the original file hash.
It intentionally exports only lighting metadata, not iCUE device settings,
actions, macros, programs, DPI or cooling settings. Alpha is distinguished from
RGB; OpenRGB's packed colour order is converted separately.

## Custom controller compatibility

The plugin normally selects modes named Direct. A narrow compatibility helper
also selects the verified per-LED Static modes on this KFA2 card, ASUS logo and
local SL-Infinity controller. It does not enable arbitrary Static controllers
for streaming. Native Direct mode always takes preference.

Tests check XML provenance/colours, exclusion of non-lighting fields, rejection
of entity declarations, deterministic animated output and palette bounds, and
the controller-selection whitelist. Real continuous playback and profile
switching still require physical validation.

ASUS ARGB-header work is deferred. Fan Control remains responsible for cooling.

Playback timing uses a scoped Windows timer request while effects run. The
Windows 11 background timer policy is preserved and restored after the final
active effect. Frames stage colours before submitting each device independently. Each device
has at most one pending job; obsolete frames are dropped. Jobs validate current
controller membership while holding the controller lifetime lock and finish
before the effect thread exits.
The driver retains its required 5 ms packet pauses using native Windows waits.
`frame_batch_test.cpp` checks staging, independent submission, busy-device frame dropping and last-write
semantics with `-Itest-stubs/frame-batch -Iplugin`. On Windows,
`timer_resolution_test.cpp` with `-Iplugin` checks timing when timer requests
are initially ignored, nested effect lifetimes and policy restoration.

## XML translator

See [the iCUE translator](icue-translator/README.md) for reusable lighting conversion,
including Halloween - The Dark and Titanium. The updated plugin adds IcueLayers.

## Keyboard ripple and combined profiles

See [Keyboard Ripple](KEYBOARD-RIPPLE.md) for the tested white/purple keyboard effect, Windows input handling and Light Mount key mapping. The profile generator preserves ValWhite as the startup default.

Combine a base profile with the keyboard effect using the helper below. It excludes the keyboard from the base effect to avoid overlapping writers and creates filenames using OpenRGB’s profile-name rules. The optional --red flag sets red ripples on a white keyboard.

```powershell
python combine-keyboard-ripple.py "C:\path\Firestorm.json" "C:\path\Keyboard Ripple.json" "C:\path\output" --name "Firestorm + Keyboard Ripple" --red
```

Existing output files are preserved. Copy the generated profile into Settings\profiles
and restart OpenRGB to refresh the menu. Saved profiles remain local; the generator
and renderer sources are versioned. Brimstone, Rainbow Wave, Titanium, Halloween,
Firestorm and the keyboard ripple have been physically checked on this setup.
The renderer targets 30 FPS; individual devices retain their own transport limits
(the six fan lighting channels update at approximately five full frames per second).

## Review corrections

Busy-device filtering is restricted to RainbowWave, Brimstone, IcueLayers and KeyRipple, which do not index cached state by assignment order. Other stock effects retain the complete stable zone list. Whole-zone colour callbacks enter per-LED frame staging so stock solid-colour effects still submit hardware updates. Some stock effects can wait on slow device geometry reads; this preserves their state safely.

zone-staging-check.py compiles the actual ControllerZone colour callback bodies against a fake device. Its four arguments are the Effects Plugin source directory, this effects directory, the compiler path and a temporary output directory. It checks whole-zone staging, single-LED overrides and one hardware submission. The converter rejects output filenames that disagree with OpenRGB’s normalized profile lookup.
