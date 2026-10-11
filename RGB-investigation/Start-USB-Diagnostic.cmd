@echo off
setlocal
set "usbDiagnosticScript=%~dp0Diagnose-USB.ps1"
echo Checking USB capture devices. Fan Control can remain running.
powershell.exe -NoProfile -Command "try { $diagnosticArguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:usbDiagnosticScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $diagnosticArguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"
echo.
echo Device check finished. Tell Codex it is done.
pause
