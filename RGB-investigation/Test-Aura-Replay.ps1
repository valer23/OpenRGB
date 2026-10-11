$ErrorActionPreference = 'Stop'
$testRoot = $PSScriptRoot
$testLog = Join-Path $testRoot 'Aura-replay-result.txt'
Start-Transcript -Path $testLog -Force
$lightingWasRunning = $false
try {
    Write-Host 'ASUS lighting replay test' -ForegroundColor Cyan
    Write-Host 'Close Armoury Crate and OpenRGB. Keep Fan Control running.'
    Write-Host 'Watch the motherboard logo, fan lighting, and GPU lighting.'
    Write-Host 'The test sends RED, GREEN, BLUE, RED, five seconds each.'
    Read-Host 'Press Enter when ready'
    $lightingService = Get-Service -Name LightingService
    $lightingWasRunning = $lightingService.Status -eq 'Running'
    if ($lightingWasRunning) {
        Stop-Service -Name LightingService -ErrorAction Stop
        (Get-Service -Name LightingService).WaitForStatus('Stopped', [TimeSpan]::FromSeconds(15))
    }
    & 'C:\Users\valerian\AppData\Local\Programs\Python\Python312\python.exe' (Join-Path $testRoot 'aura_hid.py') --replay
    if ($LASTEXITCODE -ne 0) { throw "Replay exited with code $LASTEXITCODE" }
} catch {
    Write-Host $_ -ForegroundColor Red
} finally {
    if ($lightingWasRunning) {
        Start-Service -Name LightingService
        Write-Host 'ASUS lighting service restarted.'
    }
    Stop-Transcript
}
Read-Host 'Test finished. Tell Codex which lights followed the sequence. Press Enter to close'
