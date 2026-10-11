@echo off
setlocal
for /f %%T in ('powershell.exe -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "captureStamp=%%T"
set "captureFile=%~dp0Aura-colours-%captureStamp%.pcap"
echo ASUS AURA capture: USBPcap1, device address 4 only.
echo Keep OpenRGB closed. FanControl can remain running.
echo Capture must stay active below. If Recording ended appears immediately, do not change colours.
echo Once active, set RED, GREEN, BLUE, RED with 5 seconds between colours.
echo Return here and press lowercase q to stop normally.
echo Saving to: %captureFile%
start "" /b /wait "C:\Program Files\USBPcap\USBPcapCMD.exe" -d \\.\USBPcap1 --devices 4 --inject-descriptors -o "%captureFile%" 2> "%~dp0capture-errors-%captureStamp%.txt"
echo Recording ended. Exit code: %errorlevel%
type "%~dp0capture-errors-%captureStamp%.txt"
pause
