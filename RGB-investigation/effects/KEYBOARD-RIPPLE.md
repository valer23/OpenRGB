# Keyboard Ripple (Windows, be quiet! Light Mount)

Adds a key-triggered effect to the custom Effects Plugin. Load **Keyboard Ripple**
in the bottom-right profile selector. It affects only the supported keyboard;
other devices retain the saved base profile colours. Select ValWhite to stop it.

Defaults: white base, purple (160,0,255) highlight, 100 ms pressed-key highlight,
800 ms outward ripple, 30 renderer FPS. Several presses overlap, capped at 64
waves. A held key triggers once; release and press again for another ripple.
Speed scales both the highlight and wave timing. Two user colours in the effect
panel select base and ripple. CustomSettings in the saved profile accepts
highlight_ms (0–1000), duration_ms (100–5000), width (0.5–5 key pitches).

Raw Input runs in a message-only Windows window while the effect is enabled,
including when OpenRGB is in the tray. It filters the physical keyboard's
VID_373F/PID_0002 and stores only bounded, short-lived scan-code/timestamp events
in memory. It does not log typed text or write key events to files. It stops and
unregisters input on effect stop/destruction. Ordinary keyboard input is not
suppressed. Only one ripple listener can register at a time.

The hardware reports empty key names. The initial mapping is specific to the
135-lamp, 84×7 matrix already saved from this keyboard. No other keyboard layout
is accepted. Physical Set-1 scan codes make the mapping independent of typed
characters/layout language. ISO Enter uses both lamp sections. Media keys and
unmapped scan codes are ignored; T, A, Space, arrows and typing ripples have been physically confirmed on this setup; less common keys may still need adjustment.

Generate a profile with `make-keyboard-ripple.py SOURCE_PROFILE OUTPUT_PROFILE`.
The source is read-only; existing outputs are not overwritten. Startup remains
ValWhite unless explicitly changed. The stock plugin does not include KeyRipple.

Compile key_ripple_test.cpp as C++17 with -Iplugin/KeyRipple and -luser32.
Checks cover representative scan codes, ripple falloff/timing and repeated input
listener start/stop with single-owner registration. Test failures print to the
terminal without assertion pop-ups. The custom plugin builds with Qt 6.8.3 and
MinGW 13.1 as described in the effects build instructions.

Windows input API reference:
https://learn.microsoft.com/en-us/windows/win32/inputdev/using-raw-input

Animated combinations embed a bounded `base_effect` descriptor inside the
keyboard ripple's CustomSettings. Brimstone and imported iCUE layers are sampled
as the idle background; the ripple blends over that colour before brightness
and tint adjustments. The keyboard belongs to one effect, so separate effect
threads cannot overwrite each other. Background speed is independent of ripple
speed (Firestorm retains 140%). ValWhite combinations retain their static base.
The profile generator copies supported keyboard background settings and removes
the keyboard from the standalone background assignment. Run
`python test_combine_keyboard_ripple.py` to check composition and ownership.
