@echo off
setlocal
set "xtremeTraceScript=%~dp0Capture-Xtreme-Lighting.ps1"
echo Opening the GPU lighting trace. Approve the Windows administrator prompt.
powershell.exe -NoProfile -Command "try { $traceArguments = '-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:xtremeTraceScript + [char]34; Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -ArgumentList $traceArguments -Wait -ErrorAction Stop } catch { Write-Host $_ -ForegroundColor Red; exit 1 }"
pause
