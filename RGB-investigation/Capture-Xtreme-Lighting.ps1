$ErrorActionPreference = 'Stop'
Start-Transcript -Path (Join-Path $PSScriptRoot ('Xtreme-capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-result.txt'))
try {
    Write-Host 'Observe Xtreme Tuner GPU lighting commands' -ForegroundColor Cyan
    Write-Host 'Keep Fan Control running. Close Xtreme Tuner, OpenRGB, Armoury Crate and L-Connect.'
    Write-Host 'The capture will open Xtreme Tuner and guide each colour change.'
    Read-Host 'Press Enter when ready'
    if (-not (Get-Process FanControl -ErrorAction SilentlyContinue)) { throw 'Keep Fan Control running.' }
    & 'C:\Users\valerian\AppData\Local\Programs\Python\Python312\python.exe' (Join-Path $PSScriptRoot 'capture_xtreme.py')
    if ($LASTEXITCODE -ne 0) { throw "Trace failed with exit code $LASTEXITCODE. Do not repeat colour changes." }
} catch { Write-Host $_ -ForegroundColor Red }
finally { Stop-Transcript }
Read-Host 'Press Enter to close'
