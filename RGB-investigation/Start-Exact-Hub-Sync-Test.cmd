@echo off
setlocal
set "hubTestScript=%~dp0Test-Exact-Hub-Sync.ps1"
echo Opening the exact lighting command test. Approve the Windows administrator prompt.
powershell.exe -NoProfile -Command "try { $hubArguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:hubTestScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $hubArguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"
pause
