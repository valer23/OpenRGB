@echo off
setlocal
set "fanTestScript=%~dp0Test-Fan-ARGB.ps1"
echo Opening the fan lighting test. Approve the Windows administrator prompt.
powershell.exe -NoProfile -Command "try { $fanArguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:fanTestScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $fanArguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"
echo.
echo Launcher finished.
pause
