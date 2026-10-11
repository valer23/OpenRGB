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
- ASUS motherboard logo control works; ARGB fan/GPU header sync is unresolved.
- Fan Control continues to manage cooling; these tests target lighting only.

## Run the test

Exit the installed OpenRGB and Xtreme Tuner through their tray menus.
Keep Fan Control running. Open `Start-OpenRGB-KFA2-Test.cmd` and approve the
administrator prompt. Select the KFA2 GPU, choose Static and test green,
then purple. This executable uses separate settings and contains only the
KFA2 detector. It does not replace the installed OpenRGB.

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

Run `Build-OpenRGB-KFA2.ps1`. It uses portable Qt 6.8.3 / MinGW 13.1,
Python 3.12, and official mbedTLS v3.2.1 C sources. These dependencies are
copied locally and ignored by Git. The mbedTLS archive is rebuilt with the
same compiler rather than linking the original MSVC library.

Hardware-free checks are `native-tests/kfa2_driver_test.cpp` (actual native
controller with a fake NVAPI boundary) and `test_kfa2_restore.py`.
No changes are automatically pushed. OpenRGB's upstream contribution policy
must be respected before any proposed submission.
