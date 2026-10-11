$ErrorActionPreference = 'Stop'
Start-Transcript -Path (Join-Path $PSScriptRoot ('KFA2-direct-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-result.txt'))
try {
    Write-Host 'Direct KFA2 GPU lighting replay' -ForegroundColor Cyan
    Write-Host 'Keep Fan Control running. Exit Xtreme Tuner from its tray and close OpenRGB.'
    Write-Host 'This test changes GPU lights only, then restores their previous settings.'
    Read-Host 'Press Enter when ready to watch RED, GREEN, BLUE, RED'
    if (-not (Get-Process FanControl -ErrorAction SilentlyContinue)) { throw 'Keep Fan Control running.' }
    if (Get-Process 'Xtreme Tuner','OpenRGB' -ErrorAction SilentlyContinue) { throw 'Exit Xtreme Tuner and OpenRGB first.' }
    & 'C:\Users\valerian\AppData\Local\Programs\Python\Python312\python.exe' (Join-Path $PSScriptRoot 'kfa2_nvapi.py') --replay
    if ($LASTEXITCODE -ne 0) { throw "GPU lighting test failed with exit code $LASTEXITCODE." }
} catch { Write-Host $_ -ForegroundColor Red }
finally { Stop-Transcript }
Read-Host 'Tell Codex whether the GPU followed all four colours. Press Enter to close'
