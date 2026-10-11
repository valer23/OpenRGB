$ErrorActionPreference = 'Stop'
$python = 'C:\Users\valerian\AppData\Local\Programs\Python\Python312\python.exe'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
Start-Transcript -Path (Join-Path $PSScriptRoot "LianLi-direct-$stamp-result.txt")
try {
    if (-not (Get-Process FanControl -ErrorAction SilentlyContinue)) { throw 'Keep Fan Control running.' }
    if (Get-Process 'L-Connect 3','OpenRGB','OpenRGB-KFA2-test' -ErrorAction SilentlyContinue) {
        throw 'Exit L-Connect and both OpenRGB versions from their tray menus, then run this test again. Your GPU colour will remain set.'
    }
    if ((Get-Service LConnectService).Status -ne 'Stopped') { throw 'L-Connect service must remain stopped for this test.' }
    Write-Host 'SL-Infinity lighting test. Fan Control stays running.' -ForegroundColor Cyan
    Write-Host 'First switches motherboard LIGHTING sync off, then tests direct colours.'
    Write-Host 'Only lighting commands are sent. GPU lighting and fan speeds are not changed.'
    Write-Host 'Watch fan centres AND side strips: RED, GREEN, BLUE, then PURPLE, five seconds each.'
    $fanCounts = '3,2,1,0'
    Write-Host 'Using your confirmed hub port counts: 3,2,1,0.'
    Read-Host 'Press Enter when ready to watch the fans'
    & $python (Join-Path $PSScriptRoot 'lianli_direct.py') --replay --disable-sync --fans $fanCounts
    if ($LASTEXITCODE -ne 0) { throw "Lighting test failed with exit code $LASTEXITCODE. Keep this window open and share the error." }
} catch {
    Write-Host $_ -ForegroundColor Red
} finally { Stop-Transcript }
Read-Host 'Tell Codex which fan lights changed. Press Enter to close'