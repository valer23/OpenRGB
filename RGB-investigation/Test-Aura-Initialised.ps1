$ErrorActionPreference = 'Stop'
$pythonExe = 'C:\Users\valerian\AppData\Local\Programs\Python\Python312\python.exe'
$lightingWasRunning = $false
$resultName = 'Aura-initialised-v2-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-result.txt'
Start-Transcript -Path (Join-Path $PSScriptRoot $resultName)
try {
    Write-Host 'Test ASUS lighting setup V2: includes captured post-setup command' -ForegroundColor Cyan
    Write-Host 'Keep Fan Control running. Close Armoury Crate, OpenRGB and L-Connect.'
    Write-Host 'Keep GPU MBSync enabled. Watch RED, GREEN, BLUE, RED, five seconds each.'
    Read-Host 'Press Enter when ready'
    if (-not (Get-Process FanControl -ErrorAction SilentlyContinue)) { throw 'Fan Control must remain running.' }
    if (Get-Process 'L-Connect 3','OpenRGB','ArmouryCrate' -ErrorAction SilentlyContinue) { throw 'Close the RGB apps first, then run this test again.' }
    & $pythonExe (Join-Path $PSScriptRoot 'aura_initialised_replay.py')
    if ($LASTEXITCODE -ne 0) { throw 'Captured report validation failed.' }
    $lightingWasRunning = (Get-Service LightingService).Status -eq 'Running'
    if ($lightingWasRunning) {
        Stop-Service LightingService
        (Get-Service LightingService).WaitForStatus('Stopped', [TimeSpan]::FromSeconds(15))
    }
    & $pythonExe (Join-Path $PSScriptRoot 'lianli_feature_sync.py')
    if ($LASTEXITCODE -ne 0) { throw 'Fan lighting sync command failed.' }
    & $pythonExe (Join-Path $PSScriptRoot 'aura_initialised_replay.py') --send
    if ($LASTEXITCODE -ne 0) { throw 'ASUS lighting test failed.' }
} catch {
    Write-Host $_ -ForegroundColor Red
} finally {
    if ($lightingWasRunning) { Start-Service LightingService; Write-Host 'ASUS lighting service restarted.' }
    Stop-Transcript
}
Read-Host 'Tell Codex which lights changed. Press Enter to close'
