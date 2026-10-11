$ErrorActionPreference = 'Stop'
$pythonExe = 'C:\Users\valerian\AppData\Local\Programs\Python\Python312\python.exe'
$lightingWasRunning = $false
Start-Transcript -Path (Join-Path $PSScriptRoot 'Exact-hub-sync-result.txt') -Force
try {
    Write-Host 'Test the exact L-Connect lighting-sync command' -ForegroundColor Cyan
    Write-Host 'Keep Fan Control running. Close L-Connect, Armoury Crate and OpenRGB.'
    Write-Host 'Watch the fans and motherboard for RED, GREEN, BLUE, RED.'
    Read-Host 'Press Enter when ready'
    if (-not (Get-Process FanControl -ErrorAction SilentlyContinue)) { throw 'Fan Control must remain running for this test.' }
    if (Get-Process 'L-Connect 3' -ErrorAction SilentlyContinue) { throw 'Exit L-Connect first.' }
    $lightingWasRunning = (Get-Service LightingService).Status -eq 'Running'
    if ($lightingWasRunning) {
        Stop-Service LightingService
        (Get-Service LightingService).WaitForStatus('Stopped', [TimeSpan]::FromSeconds(15))
    }
    & $pythonExe (Join-Path $PSScriptRoot 'lianli_feature_sync.py')
    if ($LASTEXITCODE -ne 0) { throw "Hub sync command failed: $LASTEXITCODE" }
    & $pythonExe (Join-Path $PSScriptRoot 'aura_hid.py') --replay
    if ($LASTEXITCODE -ne 0) { throw "Motherboard replay failed: $LASTEXITCODE" }
} catch {
    Write-Host $_ -ForegroundColor Red
} finally {
    if ($lightingWasRunning) { Start-Service LightingService; Write-Host 'ASUS lighting service restarted.' }
    Stop-Transcript
}
Read-Host 'Tell Codex whether the fans followed the colours or just froze. Press Enter to close'
