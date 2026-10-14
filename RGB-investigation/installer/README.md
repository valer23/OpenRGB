# OpenRGB Custom personal installer

Inno Setup 6.7.3 EXE installer for valerian's tested Windows installation.
Adds a Windows Installed Apps entry and uninstaller with a stable AppId.

Run `Build-Installer.ps1` with `-Compiler` pointing to ISCC.exe to rebuild.
It packages the working installation via a file allowlist, including the Effects
Plugin and all currently saved profiles. Payloads and installer binaries are ignored by
Git because they include personal device layouts. Do not distribute this personal
installer publicly. It is unsigned; Windows may show an unknown-publisher prompt.

## Upgrade behaviour

- Installs into the existing `C:\Users\valerian\Apps\OpenRGB-Custom` folder.
- Requires the valerian account and administrator elevation.
- Refuses installation/uninstallation while OpenRGB-Custom.exe is running.
- Updates application/plugin binaries and launcher scripts.
- Installs default configuration/profiles only when absent; never replaces them.
- Reuses the existing PawnIO installation (kernel driver installation not bundled).
- Replaces this installation's startup task using the same name and highest
  privilege interactive logon, with a 30-second delay and no run time limit.
- Start menu launches the same service-aware wrapper as the startup task, with
  `--gui` instead of `--startminimized`.
- Uninstall removes packaged app files, shortcut and its matching startup task.
  Settings, profiles and existing diagnostic backups are deliberately retained.

The default profile remains ValWhite. Fan Control is not modified.

Validation: installer compilation; PowerShell syntax parsing; payload SHA-256
manifest; live install verification is performed after OpenRGB is closed.

## Rebuild the installer

1. Install Inno Setup 6.7.3 from https://jrsoftware.org/isdl.php (the compiler is
   a build prerequisite, not part of OpenRGB). The original build used the
   official signed compiler with publisher Pyrsys B.V.
2. Ensure the working app is in `C:\Users\valerian\Apps\OpenRGB-Custom`.
   This script packages that installed build; it does not compile OpenRGB or
   the Effects Plugin from source. PawnIO must already be installed.
3. Open PowerShell and run:

```powershell
Set-Location 'C:\Users\valerian\git-repos\OpenRGB-KFA2\RGB-investigation\installer'
.\Build-Installer.ps1 -Compiler 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
```

Use the actual ISCC.exe location if Inno Setup was installed elsewhere.
The installer is written to `output\OpenRGB-Custom-Setup-1.0.20261013.2.exe`.
Before a new release, increment AppVersion in OpenRGB-Custom.iss; retain AppId
so Windows recognizes it as the same application. The payload directory is a
staging area; inspect it for stale files when changing the packaged file set.

## Install or upgrade

1. Save any profile changes and exit OpenRGB Custom from its tray menu.
   Keep Fan Control running.
2. Back up `Settings\OpenRGB.json`, the `Settings\profiles` directory and
   `Start-OpenRGB-Custom.ps1` outside the app directory. For task recovery,
   export `OpenRGB Custom (valerian)` from Windows Task Scheduler.
3. Run the generated installer under the valerian account and approve UAC.
4. Launch OpenRGB Custom from Start, or run its task in Task Scheduler.
   The task launches minimized; open the window from the tray icon.
5. Confirm the Windows Installed Apps entry, two RAM entries, keyboard, fans,
   mouse, ASUS logo and KFA2 GPU. Check ValWhite and an animated profile.

An upgrade preserves existing lighting settings. Version 1.0.20261013.2 migrates only the former KFA2 NVAPI location/description to the standard I2C identifier, with a backup outside the profile directory. It keeps existing OpenRGB.json settings, except it disables the built-in AutoStart flag to prevent a competing non-elevated launch. Therefore
it does not automatically replace an existing effect profile with a newer default.
Install a revised profile separately after backing up the old one when required.
If the app is already running, open it from its tray icon; the launcher avoids
starting a second instance.

## Uninstall and recovery

Close OpenRGB from its tray menu, then use Windows Settings > Apps > Installed
Apps > OpenRGB Custom > Uninstall. The installer removes its packaged files and
matching startup task. User settings/profiles and old diagnostic backups remain.
It does not uninstall the shared PawnIO driver or modify Fan Control.

To recover, reinstall the previous installer and restore backed-up configuration
or profiles only if needed. Use the Start menu shortcut or startup task to launch
so ASUS LightingService is paused/restored correctly. Avoid launching the bare
EXE when ASUS's lighting service is running. If a forced termination prevents
service restoration, start LightingService from Windows Services.

## Verified installation — 12 October 2026

Version 1.0.20261012.1 was installed successfully and registered in Installed Apps.
All seven saved profile files matched their pre-install SHA-256 hashes. The
startup task launched successfully, detected seven controllers, registered 60
plugin effects and loaded ValWhite. No Windows restart was required.
Uninstallation and a second upgrade have not been exercised end-to-end.

The first compiler pass accepted an unsupported `{userprofile}` runtime constant;
installation stopped before changing files. The retained installer uses
`GetEnv('USERPROFILE')` instead and is the successfully installed revision.

Installer sources and build scripts are versioned in this repository. Keep
personal payloads, recovery copies and installer binaries out of Git.

## Reboot startup repair — 12 October 2026

OpenRGB’s own AutoStart setting created a non-elevated launch without the custom settings path. That instance started before the administrator task, which then skipped launch because OpenRGB was already running. Configure-Startup.ps1 now disables this flag and backs up any matching direct startup shortcut. Keep OpenRGB Custom (valerian) as the startup task. The obsolete OpenRGB task pointing to the removed official installation can be deleted manually.

The repaired launch successfully initialized PawnIO and loaded ValWhite without permission-denied errors. Installer version 1.0.20261012.2 includes this prevention; the live installation was repaired directly.

## Refreshed package � 13 October 2026

Version 1.0.20261013.1 packages the installed, tested Effects Plugin with
animated keyboard backgrounds for Brimstone, Firestorm and Titanium plus typing
ripples. ValWhite + Keyboard Ripple keeps its white idle background. All eleven
saved profiles are included as defaults for missing files. Existing profiles
and startup selection are preserved during upgrades. The owner confirmed all
three animated/ripple combinations on the keyboard before packaging.

This refresh builds a replacement installer; the live plugin/profiles already
contain the fix. Running the installer is not needed for the current session.
Upgrade/uninstall testing remains separate from installer compilation.

## Shared-I2C package refresh

Version 1.0.20261013.2 packages the owner-verified shared-I2C GPU build. The
GPU controller now uses i2c_smbus_interface and the standard PCI/I2C detector.
Local fake-bus checks, native shared-backend red/green/blue/purple/restoration
and the full Windows GUI/profile checks passed. The installer migrates exact
old GPU identifiers on this personal GPU-0 installation, after preserving a
backup; effect settings are unchanged. Installer compilation does not verify
Linux or end-to-end upgrade/uninstall behavior.

## Galax v1 package refresh - 15 October 2026

Version 1.0.20261015.1 packages the owner-verified Galax v1 integration for the
KFA2 RTX 3080 Ti EX Gamer. The standalone controller is removed. Existing saved
profile identity and mode indices are retained, so this revision requires no
additional profile migration. Native colour/restoration and full GUI profile
checks passed on Windows. Linux hardware and end-to-end upgrade/uninstall
behavior remain unverified.

Version 1.0.20261015.2 contains the small Galax EX cleanup: detection is logged
at debug level, repeated write errors are suppressed until a complete update
succeeds, and controller callbacks rely on the base device lock. Register
commands and timing are unchanged. Local failure/recovery regression checks
and the full Windows build passed.
