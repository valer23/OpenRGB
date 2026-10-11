@echo off
setlocal
set "kfa2OpenRGBScript=%~dp0Open-OpenRGB-Custom.ps1"
echo Opening the GPU and fan OpenRGB test build. Approve the Windows administrator prompt.
powershell.exe -NoProfile -Command "try { $testArguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:kfa2OpenRGBScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $testArguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"
