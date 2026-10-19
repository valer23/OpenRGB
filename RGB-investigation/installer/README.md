# OpenRGB Custom personal installer

## Device-specific mouse idle settings - 19 October 2026

Version 1.0.20261019.2 moves the idle handoff toggle and timeout to the mouse's
Edit Device page. OpenRGB saves them through its existing Configuration.json
mechanism. The driver no longer reads CorsairMouseIdle from OpenRGB.json.
Upstream defaults to disabled/900 seconds; this personal package migrates the
existing opt-in and timeout (normally enabled/60 seconds).

Configure-Mouse-Idle.ps1 backs up both settings files, writes the per-device
entry first, then removes the old application property. Existing valid device
preferences win. Malformed new settings stop migration without deleting the
legacy fallback. Existing fan configuration and all profile colours/effects
are preserved; a saved ValWhite mouse description supplies the matching device
identity if Configuration.json has no mouse entry yet. Repeated runs are safe.

The Windows build, independent standards and behaviour reviews, driver
failure/recovery checks and migration fixtures passed. The isolated GUI
confirmed the controls, disabled handoff staying animated, and enabled/60-second
handoff plus movement resume after correcting the checkbox schema to OpenRGB's
"bool" type. An actual-widget check covers checkbox callbacks and timeout limits.
Version 1.0.20261019.2 is installed. Its hashes, enabled/60-second migration,
unchanged fan configuration and all profile colours/effects passed verification.
Full firmware
sleep and battery-life measurement remain separate, pending checks. Linux and
macOS are unverified. Older sections below describe historical packages.

## Native mouse idle revision - 19 October 2026

Version 1.0.20261019.1 replaces Windows Raw Input tracking with the
Dark Core RGB Pro SE's firmware inactivity counter (GET 0x40) and Slipstream
receiver connection notifications (0x36). It reuses the existing Corsair V2
controller, render commands, update queue and keepalive workers. It adds no
input listener or worker thread and does not change firmware sleep settings.
The existing opt-in settings and personal installer's 60-second default remain.

The event interface must belong to the controlled receiver. Windows uses the
bundled HIDAPI container ID; other systems require a unique, nonempty receiver
serial match. Capability or interface validation failure at startup leaves the
original lighting path enabled. Counter reads are limited to once per second
while connected; known-offline mice receive no lighting frames, queries or
keepalives. Reader failures pause mouse traffic until receiver state is checked.
Wake initialization is checked and retried at most once per second.

Changing rendering mode resets the firmware inactivity counter. The driver
keeps the idle handoff latched across this reset, and resumes on subsequent
counter evidence of activity or a receiver reconnect. The counter has roughly
one-second resolution. A very brief input immediately after handoff can be
indistinguishable from the mode reset and may be missed; this is not equivalent
to operating-system input tracking. The code allows two seconds for counter
quantization and transport timing when comparing inferred activity times.

The Windows build, independent correctness/reuse review and private actual-driver
regressions passed. Those regressions include reset-to-zero, three minutes idle,
sustained input, per-receiver matching, offline silence and bounded recovery.
The owner confirmed a two-minute full-application handoff and movement resume,
and all trial effects including the GPU after correcting copied GPU identities.
The final full-application lights-off sleep/wake check is still pending. Earlier
sleep/wake results below apply to the installed Raw Input version or to the
standalone ten-second counter probe, not this one-second native app revision.
A brief input immediately after handoff also restored effects in the owner's
short check; this does not guarantee detection for every subsecond input.
Linux/macOS builds and hardware and battery-life improvement remain unverified.
Version 1.0.20261019.1 was installed for longer personal validation. Installed
application/plugin hashes, all profile colours/effects, OpenRGB settings and
persistent device configuration passed post-install verification. The installer
migrates the old SG GPU identity to the tested common Galax controller using
the existing transactional profile migration. A full pre-upgrade application,
profile/settings and startup-task backup is retained in the private upgrade
folder; the previous 1.0.20261018.1 installer remains available for rollback.

## Mouse idle update - 18 October 2026

Version 1.0.20261018.1 adds an opt-in Windows idle handoff for the Dark Core
RGB Pro SE on Slipstream. This personal installer enables it with a 60-second
mouse inactivity timeout. Firmware then owns lighting and OpenRGB suppresses
lighting traffic until mouse movement, buttons or wheel input resumes effects.
The firmware's own sleep timeout is unchanged. Keyboard input does not reset
the mouse timer. Other supported devices and wired operation retain their
existing lighting path.

In the historical 18 October package, Configure-Mouse-Idle.ps1 backed up OpenRGB.json before adding CorsairMouseIdle.
It preserves an existing block, including an explicit enabled=false opt-out or
custom timeout. To change it, exit OpenRGB, edit timeout_seconds (5 to 3600),
and restart. Missing settings in the driver preserve its former behaviour;
installing this personal package supplies the opt-in. Profiles are unchanged.

The owner tested the isolated 30-second diagnostic on 18 October: effects
changed to onboard lighting after inactivity, resumed on movement, and after
16-17 minutes the mouse lights turned off completely and movement restored
Rainbow Wave. This confirms sleep/wake behaviour, not a measured battery-life
improvement. Private policy, Raw Input and actual-driver/fake-HID checks passed.
Independent review also covered a signed HID-read error correction to avoid
an endless packet-drain loop on disconnect. The final Windows build includes
that correction; the physical trial preceded it. Linux/macOS were not tested.

The installed package subsequently passed its 60-second handoff/wake check,
with all saved profile hashes and persistent device configuration preserved.

Pre-submission review found two additional cases: multiple Slipstream receivers
must not share activity, and a failed lighting-initialization response must keep
wake recovery pending. The source now correlates interfaces using bundled
HIDAPI container IDs and retries until both existing initialization steps pass.
Independent re-review, the Windows build, native listener/container checks and
private initialization-failure/retry checks passed. These review revisions are
not included in the installed 1.0.20261018.1 package and have not had another
full-application physical sleep/wake test. The original hardware result above
applies to the installed version. The upstream submission contains only the six
controller files, without this personal packaging or the private tests.

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

## SG upgrade - 16 October 2026

Version 1.0.20261016.1 packages the reviewed Galax v1 correction for the
KFA2 GeForce RTX 3080 Ti SG 1-Click OC, SKU 38IOM5MD99DK. The former EX Gamer
name was incorrect. The source was independently reviewed, built on Windows
and physically verified in an isolated app before packaging.

The upgrade backs up changed profiles before replacing their old GPU identity
and Static mode label with SG and Direct. It also migrates the former local fan
test controller to the standard SL-Infinity controller and its Custom streaming
mode. Existing per-LED colours, effect parameters, channel counts and unrelated
devices are retained. The plugin accepts that controller's per-LED Custom mode.

Fan sizes are persistent controller configuration, not just lighting state. If
Configuration.json lacks the migrated hub, its sizes are seeded from ValWhite,
with configured-size flags. Existing unrelated controller configuration is kept.
The post-install migration seeds this configuration on fresh installations too.
It does not install a default configuration before migration, so the backup
manifest accurately records whether Configuration.json originally existed.

Migration parses all candidates before writing, verifies backups, uses atomic
file replacement and rolls back files already written if a later write fails.
Backups and an original-existence manifest are in
Settings/profile-backups/before-sg-upgrade/<run-id>. A repeated run makes no
further changes. For rollback, restore originally existing files from that run
and remove only a Configuration.json marked originally absent in its manifest.

Build-Installer.ps1 accepts -AppRoot for a prepared staging folder. This release
stages the tested binary and plugin with copies of the current settings, migrates
those copies, then packages them. It does not package an older installed EXE.
Personal staging folders, backups and installer binaries remain excluded from Git.
Persistence across a power cycle and Linux/macOS hardware are still unverified.

Live upgrade verified on 17 October 2026: Windows Installed Apps reports
1.0.20261016.1, the installed application/plugin hashes match the tested package,
and all eleven live profiles preserve their LED colours, effect settings and
unrelated controllers compared with pre-install backups. OpenRGB detects one SG
GPU and two RAM sticks; persistent fan sizes are 48/48, 32/32, 16/16 and 0/0.
The user checked the requested static, animated and keyboard-ripple profiles and
reported everything working without a zone-initialisation prompt. The scheduled
launcher restarted successfully. A reboot of this release and uninstall remain
untested; this does not establish device-side colour persistence.



Validation for this release: installer compilation, PowerShell syntax parsing,
payload SHA-256 checks, live profile-preservation checks and the user-confirmed
profile test after installation.

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
The installer is written to `output\OpenRGB-Custom-Setup-1.0.20261016.1.exe`.
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

## Refreshed package - 13 October 2026

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

Version 1.0.20261015.3 includes the fan and ASUS cleanup. Fan colour failures
stop before commit; commit delivery is checked. Fan errors are suppressed per
channel until a complete update succeeds. ASUS uses the base device lock and
suppresses repeated errors until a complete logo update succeeds. Successful
lighting commands and waits are unchanged. Local failure/recovery checks and
the complete Windows build passed.
