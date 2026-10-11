@echo off
setlocal
set "auraStartupScript=%~dp0Capture-Aura-Startup.ps1"
echo Opening the ASUS startup capture. Approve the Windows administrator prompt.
powershell.exe -NoProfile -Command "try { $auraArguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:auraStartupScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $auraArguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"
pause
