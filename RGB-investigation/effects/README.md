# Rainbow Wave and Brimstone

The local OpenRGB Effects Plugin is based on official source commit
`8288ef45eb27d98a237b11ae21fa76be17978c2a` (plugin API 5), built with the same
Qt 6.8.3 / MinGW 13.1 runtime as the custom OpenRGB application.

## Profiles

- **Rainbow Wave** uses the plugin's existing effect, shared timing across all
  assigned zones, and 10 FPS initially. Multi-LED zones show a wave; the GPU
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
