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
    Write-Host 'Only lighting commands are sent. GPU lighting is not changed.'
    Write-Host 'Watch fan centres AND side strips: RED, GREEN, BLUE, then PURPLE, five seconds each.'
    Write-Host 'Enter the number of fans on each physical hub port. Example: 3,2,1,0'
    Write-Host 'Earlier L-Connect configuration recorded 3,2,1,0. Confirm your actual connections.'
    $fanCounts = Read-Host 'Fans on ports 1,2,3,4'
    if ($fanCounts -notmatch '^[0-4],[0-4],[0-4],[0-4]$' -or $fanCounts -eq '0,0,0,0') {
        throw 'Use four counts from 0 to 4, separated by commas. No hardware changes made.'
    }
    Read-Host 'Press Enter when ready to watch the fans'
    & $python (Join-Path $PSScriptRoot 'lianli_direct.py') --replay --fans $fanCounts
    if ($LASTEXITCODE -ne 0) { throw "Lighting test failed with exit code $LASTEXITCODE. Keep this window open and share the error." }
} catch {
    Write-Host $_ -ForegroundColor Red
} finally { Stop-Transcript }
Read-Host 'Tell Codex which fan lights changed. Press Enter to close'