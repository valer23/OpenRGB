# iCUE lighting translator (local OpenRGB extension)

Converts `.cueprofile` XML into an OpenRGB JSON profile using the **IcueLayers**
effect supplied with this custom Effects Plugin build. A stock plugin does not
contain this effect. Python 3.10+; standard library only.

```powershell
python translate_icue.py "C:\path\input.cueprofile" --layout "C:\path\ValWhite.json" --output "C:\path\New effect.json"
```

The input and layout are read-only. Existing output files are not overwritten.
Copy the generated profile into OpenRGB's `Settings\profiles` folder after the
updated plugin is installed. Load it with the bottom-right profile selector.
Keep the `.conversion-report.txt` file outside that folder. It explains the
selected source stack, output zones and limitations. Startup stays ValWhite.

Use `--inspect` to print all source lighting stacks; `--stack N` selects a stack
other than the default most frequently serialized stack. `--name` supplies a
display name. Stack frequency describes the export, not intended brightness or
number of simultaneous layers.

## Initial supported translation

- Static colours and gradient timelines retain ARGB values, alpha and duration.
- Colour waves, pulses, shifts, rain and geometric waves use adapted renderers.
- Layer order is composited bottom to top. Numeric LED selection masks are
  normalized to each target zone; named key masks are not exact spatial mappings.
- Matrix zones use row/column coordinates; linear zones use LED order. Single
  zone devices (GPU and motherboard logo) sample the centre of the animation.
- The same selected source stack is applied to all eligible target zones.
- Direct-capable devices and the three supported local driver families are
  eligible. Unsupported devices are reported and excluded from the effect.

**This is a reusable approximate lighting translator, not a complete iCUE
emulator.** Speed enum periods, moving beam geometry, rain randomness and blend
behaviour are adaptations. The keyboard-specific scenes/artwork, reactive
keypress effects and device-specific Corsair layouts are not reproduced.
Unknown effects and random palettes are skipped with a report. No macros, DPI,
cooling, executable paths or other non-lighting settings are imported.

The supplied Halloween and Titanium examples use three and four layers
respectively. Halloween retains purple colours and timed white lightning;
Titanium retains grey background, rain and opposing waves. The videos are visual
references, not inputs to the translation algorithm.

## Checks

`python -m unittest discover -s . -p test_translate.py` checks ARGB decoding,
serialized references, XML rejection, stack order, masks and layout preservation.
Compile `pattern_test.cpp` as C++17 with the `plugin/IcueLayers` include directory
to check animation timing, opacity, mask selection, wave movement and bounds.

`IcueLayers` uses the existing independent-device renderer. No changes to USB,
I2C, fan speeds, hardware drivers or startup configuration are required.
