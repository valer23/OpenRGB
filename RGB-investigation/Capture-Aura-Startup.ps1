$ErrorActionPreference = 'Stop'
$usbExe = 'C:\Program Files\USBPcap\USBPcapCMD.exe'
$usbEnumExe = 'C:\Program Files\Wireshark\extcap\USBPcapCMD.exe'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$captureFile = Join-Path $PSScriptRoot "Aura-startup-$stamp.pcap"
$captureProcess = $null
$lightingWasRunning = $false
Start-Transcript -Path (Join-Path $PSScriptRoot 'Aura-startup-result.txt') -Force
try {
    Write-Host 'Capture ASUS lighting initialisation' -ForegroundColor Cyan
    Write-Host 'Keep Fan Control running. Close Armoury Crate, OpenRGB and L-Connect.'
    Read-Host 'Press Enter when ready'
    if (-not (Get-Process FanControl -ErrorAction SilentlyContinue)) { throw 'Fan Control should remain running.' }
    $lightingWasRunning = (Get-Service LightingService).Status -eq 'Running'
    if (-not $lightingWasRunning) { throw 'Expected ASUS LightingService to be running. No service state changed.' }
    $rootPath = $null
    $deviceAddress = $null
    foreach ($rootIndex in 1..4) {
        $candidate = '\\.\USBPcap' + $rootIndex
        $inventoryFile = Join-Path $PSScriptRoot "Aura-startup-$stamp-root$rootIndex.txt"
        & $usbEnumExe --extcap-config --extcap-interface $candidate 2>&1 | Out-File -LiteralPath $inventoryFile
        foreach ($line in (Get-Content -LiteralPath $inventoryFile)) {
            if ($line -match 'display=AURA LED Controller.*\{parent=(\d+)\}') {
                if ($rootPath) { throw 'Multiple ASUS lighting controllers found.' }
                $rootPath = $candidate
                $deviceAddress = $Matches[1]
            }
        }
    }
    if (-not $rootPath) { throw 'ASUS lighting capture interface not found.' }
    Stop-Service LightingService
    (Get-Service LightingService).WaitForStatus('Stopped', [TimeSpan]::FromSeconds(15))
    $captureArguments = '-d ' + $rootPath + ' --devices ' + $deviceAddress + ' --inject-descriptors -o "' + $captureFile + '"'
    $captureProcess = Start-Process $usbExe -ArgumentList $captureArguments -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $PSScriptRoot "Aura-startup-$stamp-errors.txt")
    Start-Sleep -Seconds 2
    if ($captureProcess.HasExited) { throw 'Capture stopped before ASUS service startup.' }
    Write-Host "Recording only ASUS controller: $rootPath address $deviceAddress"
    Start-Service LightingService
    Write-Host 'ASUS lighting restarted. Recording for 30 seconds; no clicks needed.'
    Start-Sleep -Seconds 30
    Stop-Process -Id $captureProcess.Id
    $captureProcess.WaitForExit()
    $captureProcess = $null
    Write-Host "Saved: $captureFile" -ForegroundColor Green
} catch {
    Write-Host $_ -ForegroundColor Red
} finally {
    if ($captureProcess -and -not $captureProcess.HasExited) { Stop-Process -Id $captureProcess.Id }
    if ($lightingWasRunning -and (Get-Service LightingService).Status -ne 'Running') { Start-Service LightingService }
    Stop-Transcript
}
Read-Host 'Capture finished. Press Enter to close and tell Codex it is done'
