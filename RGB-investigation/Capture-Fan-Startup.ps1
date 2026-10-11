$ErrorActionPreference = 'Stop'
$fanExe = 'C:\Program Files (x86)\FanControl\FanControl.exe'
$usbCaptureExe = 'C:\Program Files\USBPcap\USBPcapCMD.exe'
$usbEnumExe = 'C:\Program Files\Wireshark\extcap\USBPcapCMD.exe'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$captureFile = Join-Path $PSScriptRoot "Fan-startup-$stamp.pcap"
$captureProcess = $null
Start-Transcript -Path (Join-Path $PSScriptRoot 'Fan-startup-result.txt') -Force
try {
    Write-Host 'Capture Lian Li lighting-sync command' -ForegroundColor Cyan
    Write-Host 'Close L-Connect and OpenRGB. Exit Fan Control from its tray menu.'
    Read-Host 'Press Enter once Fan Control has exited'
    if (Get-Process FanControl -ErrorAction SilentlyContinue) { throw 'Fan Control is still running. No capture started.' }
    $captureRoot = $null
    $deviceAddress = $null
    foreach ($rootIndex in 1..4) {
        $rootPath = '\\.\USBPcap' + $rootIndex
        # Explicit redirection waits for this GUI-subsystem executable and
        # captures its output, matching the working USB diagnostic invocation.
        $inventoryPath = Join-Path $PSScriptRoot "Fan-startup-$stamp-root$rootIndex.txt"
        & $usbEnumExe --extcap-config --extcap-interface $rootPath 2>&1 | Out-File -LiteralPath $inventoryPath
        $inventory = Get-Content -LiteralPath $inventoryPath
        foreach ($line in $inventory) {
            if ($line -match 'display=LianLi-SL-infinity.*\{parent=(\d+)\}') {
                if ($captureRoot) { throw 'Multiple Lian Li hubs found; refusing ambiguous capture.' }
                $captureRoot = $rootPath
                $deviceAddress = $Matches[1]
            }
        }
    }
    if (-not $captureRoot) { throw 'Could not identify the Lian Li capture interface.' }
    Write-Host "Capturing only the Lian Li hub: $captureRoot address $deviceAddress"
    $captureArguments = '-d ' + $captureRoot + ' --devices ' + $deviceAddress + ' --inject-descriptors -o "' + $captureFile + '"'
    $captureProcess = Start-Process $usbCaptureExe -ArgumentList $captureArguments -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $PSScriptRoot "Fan-startup-$stamp-errors.txt")
    Start-Sleep -Seconds 2
    if ($captureProcess.HasExited) { throw 'Capture process exited before Fan Control startup.' }
    Start-Process $fanExe -WorkingDirectory 'C:\Program Files (x86)\FanControl'
    Write-Host 'Fan Control restarted. Recording for 30 seconds; no colour changes needed.'
    Start-Sleep -Seconds 30
    Stop-Process -Id $captureProcess.Id
    $captureProcess.WaitForExit()
    $captureProcess = $null
    Write-Host "Saved: $captureFile" -ForegroundColor Green
} catch {
    Write-Host $_ -ForegroundColor Red
} finally {
    if ($captureProcess -and -not $captureProcess.HasExited) { Stop-Process -Id $captureProcess.Id }
    if (-not (Get-Process FanControl -ErrorAction SilentlyContinue)) {
        Start-Process $fanExe -WorkingDirectory 'C:\Program Files (x86)\FanControl'
    }
    Stop-Transcript
}
Read-Host 'Capture finished. Press Enter to close, then tell Codex it is done'
