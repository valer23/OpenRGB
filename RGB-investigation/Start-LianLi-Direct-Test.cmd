@echo off
setlocal
set "lianLiTestScript=%~dp0Test-LianLi-Direct.ps1"
echo Opening the fan lighting test. Approve the Windows administrator prompt.
powershell.exe -NoProfile -Command "try { $testArguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:lianLiTestScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $testArguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"