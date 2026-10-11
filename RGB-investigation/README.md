# KFA2 RGB investigation

This repository retains the official OpenRGB history, with local experimental
KFA2 RTX 3080 Ti EX Gamer support on branch `kfa2-ex-captured`.
The official GitLab repository is the read-only reference remote `upstream`.
Nothing has been published or submitted upstream.

## Current status

- Independent replay physically passed red / green / blue / red.
- The native OpenRGB driver and GUI build successfully.
- Read-only application detection identifies the KFA2 card correctly.
- The user confirmed native GUI colour control works on 2026-10-11.
- The user confirmed direct SL-Infinity colours after lighting sync OFF; fans finish purple.
- The combined native build detects GPU and fans; GUI fan colour confirmation is next.
- Fan Control continues to manage cooling; these tests target lighting only.

## Run the test

Exit the installed OpenRGB and Xtreme Tuner through their tray menus.
Keep Fan Control running. Open `Start-OpenRGB-Custom.cmd` and approve the
administrator prompt. Select the KFA2 GPU, choose Static and test green,
then purple. With -IncludeFans, this executable uses separate settings and contains only
the KFA2 and SL-Infinity detectors. It does not replace the installed OpenRGB.

## Repository contents

Controller changes are in `Controllers/GalaxGPUController`; shared standard
NVAPI wrappers are in `dependencies/NVFC`. Protocol findings, scripts and
hardware-free checks are in this folder. See `OpenRGB-KFA2-integration.md`
and `Xtreme-lighting-investigation.md` for evidence and limits.

Raw captures, logs, downloaded tools, generated libraries, executable files
and test settings remain available locally but are ignored by Git. Some older
scripts require the original Fan Control investigation folders and are retained
as historical diagnostics; use the KFA2 launcher for the current test.

## Rebuild

Run `Build-OpenRGB-Custom.ps1 -IncludeFans` for the combined lighting test. It uses portable Qt 6.8.3 / MinGW 13.1,
Python 3.12, and official mbedTLS v3.2.1 C sources. These dependencies are
copied locally and ignored by Git. The mbedTLS archive is rebuilt with the
same compiler rather than linking the original MSVC library.

Hardware-free checks are `native-tests/kfa2_driver_test.cpp` (actual native
controller with a fake NVAPI boundary) and `test_kfa2_restore.py`.
No changes are automatically pushed. OpenRGB's upstream contribution policy
must be respected before any proposed submission.

## Full application and permanent installation

Build-OpenRGB-Custom-Full.ps1 enables CONFIG+=custom_full and keeps all standard
controller sources. SLINF_LOCAL_TEST replaces only the normal SL-Infinity
registration with the verified local layout/effects; other detectors remain
standard. These personal build changes are excluded from the driver MRs.

Install-OpenRGB-Custom.ps1 copies the complete deployed runtime to
C:\Users\valerian\Apps\OpenRGB-Custom and preserves settings separately in
C:\Users\valerian\Apps\OpenRGB-Custom\Settings. It adds the OpenRGB Custom
Start menu entry. The purple profile is copied from the isolated test settings.

Configure-OpenRGB-Custom.ps1 runs elevated device discovery and registers the
OpenRGB Custom (valerian) login task with a 30-second delay, highest privileges,
and ValWhite profile loading. Save ValWhite before running this setup. The GUI's built-in Auto Start remains disabled
to keep one startup mechanism. Fan Control remains running and controls cooling.

A successful build restores available standard detectors; it does not add
support for ASUS controller 0B05:1CD2 or the ROG Delta II headset. Their device
control still needs separate protocol/driver work. Physical verification and
saving a profile containing newly discovered devices remain user test steps.
