@echo off
setlocal
set "fanCaptureScript=%~dp0Capture-Fan-Startup.ps1"
echo Opening the Lian Li diagnostic. Approve the Windows administrator prompt.
powershell.exe -NoProfile -Command "try { $captureArguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:fanCaptureScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $captureArguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"
pause
