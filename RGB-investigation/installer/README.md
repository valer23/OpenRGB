# OpenRGB Custom personal installer

Inno Setup 6.7.3 EXE installer for valerian's tested Windows installation.
Adds a Windows Installed Apps entry and uninstaller with a stable AppId.

Run `Build-Installer.ps1` with `-Compiler` pointing to ISCC.exe to rebuild.
It packages the working installation via a file allowlist, including the Effects
Plugin and seven saved profiles. Payloads and installer binaries are ignored by
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
The installer is written to `output\OpenRGB-Custom-Setup-1.0.20261012.1.exe`.
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

An upgrade preserves existing profiles and OpenRGB.json byte-for-byte. Therefore
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
