@echo off
setlocal
set "kfa2TestScript=%~dp0Test-KFA2-Direct.ps1"
echo Opening the direct GPU lighting test. Approve the Windows administrator prompt.
powershell.exe -NoProfile -Command "try { $kfa2Arguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:kfa2TestScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $kfa2Arguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"
pause
