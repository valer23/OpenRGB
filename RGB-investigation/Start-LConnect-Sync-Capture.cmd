@echo off
setlocal
set "lcCaptureScript=%~dp0Capture-LConnect-Sync.ps1"
echo Opening the vendor lighting test. Approve the Windows administrator prompt.
powershell.exe -NoProfile -Command "try { $lcArguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:lcCaptureScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $lcArguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"
pause
