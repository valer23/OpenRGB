@echo off
setlocal
set "auraSetupScript=%~dp0Test-Aura-Initialised.ps1"
echo Opening the ASUS lighting setup test. Approve the Windows administrator prompt.
powershell.exe -NoProfile -Command "try { $auraSetupArguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:auraSetupScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $auraSetupArguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"
pause
