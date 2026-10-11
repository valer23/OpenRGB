$ErrorActionPreference = 'Stop'
$fanExe = 'C:\Program Files (x86)\FanControl\FanControl.exe'
$lcExe = 'C:\Program Files\Lian-Li\L-Connect 3\L-Connect 3.exe'
$usbExe = 'C:\Program Files\USBPcap\USBPcapCMD.exe'
$usbEnumExe = 'C:\Program Files\Wireshark\extcap\USBPcapCMD.exe'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$captureFile = Join-Path $PSScriptRoot "LConnect-sync-$stamp.pcap"
$captureProcess = $null
$serviceChanged = $false
Start-Transcript -Path (Join-Path $PSScriptRoot 'LConnect-sync-result.txt') -Force
try {
    Write-Host 'Compare the L-Connect lighting-sync command' -ForegroundColor Cyan
    Write-Host 'Close OpenRGB and L-Connect, then exit Fan Control from its tray menu.'
    Read-Host 'Press Enter when those apps are closed'
    if (Get-Process FanControl -ErrorAction SilentlyContinue) { throw 'Fan Control is still running.' }
    if (Get-Process 'L-Connect 3' -ErrorAction SilentlyContinue) { throw 'L-Connect is still running. Exit it from the tray menu.' }
    $originalService = Get-Service LConnectService
    if ($originalService.Status -ne 'Stopped' -or $originalService.StartType -ne 'Disabled') {
        throw 'Expected the L-Connect service to be stopped and disabled. No service settings changed.'
    }
    $rootPath = $null
    $hubAddress = $null
    foreach ($rootIndex in 1..4) {
        $candidate = '\\.\USBPcap' + $rootIndex
        $inventoryFile = Join-Path $PSScriptRoot "LConnect-sync-$stamp-root$rootIndex.txt"
        & $usbEnumExe --extcap-config --extcap-interface $candidate 2>&1 | Out-File -LiteralPath $inventoryFile
        foreach ($line in (Get-Content -LiteralPath $inventoryFile)) {
            if ($line -match 'display=LianLi-SL-infinity.*\{parent=(\d+)\}') {
                if ($rootPath) { throw 'Multiple Lian Li hubs found.' }
                $rootPath = $candidate
                $hubAddress = $Matches[1]
            }
        }
    }
    if (-not $rootPath) { throw 'No Lian Li capture interface found.' }
    $captureArguments = '-d ' + $rootPath + ' --devices ' + $hubAddress + ' --inject-descriptors -o "' + $captureFile + '"'
    $captureProcess = Start-Process $usbExe -ArgumentList $captureArguments -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $PSScriptRoot "LConnect-sync-$stamp-errors.txt")
    Start-Sleep -Seconds 2
    if ($captureProcess.HasExited) { throw 'Capture stopped before L-Connect startup.' }
    Set-Service LConnectService -StartupType Manual
    $serviceChanged = $true
    Start-Service LConnectService
    Start-Process $lcExe -WorkingDirectory 'C:\Program Files\Lian-Li\L-Connect 3' -WindowStyle Normal
    Write-Host 'Recording. In L-Connect open Quick/Sync Lighting > MB Lighting Sync.' -ForegroundColor Cyan
    Write-Host 'If a hardware-change warning appears, choose Remind Me Next Boot.'
    Write-Host 'Set the SL Infinity Status checkbox OFF, then Apply. Wait five seconds.'
    Write-Host 'Set it ON, then Apply. Wait five seconds and note whether the fan lighting changes.'
    Write-Host 'Set it OFF again, then Apply. Keep motherboard fan/RPM sync off.'
    Write-Host 'Exit L-Connect from its tray menu after these steps.'
    Read-Host 'Press Enter here once L-Connect has exited'
    while (Get-Process 'L-Connect 3' -ErrorAction SilentlyContinue) {
        Read-Host 'L-Connect is still running. Exit it from the tray menu, then press Enter'
    }
    Stop-Process -Id $captureProcess.Id
    $captureProcess.WaitForExit()
    $captureProcess = $null
    Write-Host "Saved: $captureFile" -ForegroundColor Green
} catch {
    Write-Host $_ -ForegroundColor Red
} finally {
    if ($captureProcess -and -not $captureProcess.HasExited) { Stop-Process -Id $captureProcess.Id }
    if ($serviceChanged) {
        Stop-Service LConnectService -ErrorAction Continue
        Set-Service LConnectService -StartupType Disabled
        Write-Host 'L-Connect service returned to stopped/disabled.'
    }
    # Return to the original plugin after the unsuccessful ARGB plugin test.
    $regular = 'C:\Program Files (x86)\FanControl\Plugins\FanControl.LianLiPlugin.dll'
    $regularBackup = 'C:\Program Files (x86)\FanControl\Plugins\FanControl.LianLiPlugin.dll.before-ARGB-test'
    $argb = 'C:\Program Files (x86)\FanControl\Plugins\FanControl.LianLiPlugin.ARGB.dll'
    $argbInactive = 'C:\Program Files (x86)\FanControl\Plugins\FanControl.LianLiPlugin.ARGB.dll.notused'
    if (-not (Get-Process FanControl -ErrorAction SilentlyContinue)) {
        if ((Test-Path -LiteralPath $regularBackup) -and (Test-Path -LiteralPath $argb) -and -not (Test-Path -LiteralPath $regular) -and -not (Test-Path -LiteralPath $argbInactive)) {
            Rename-Item -LiteralPath $argb -NewName 'FanControl.LianLiPlugin.ARGB.dll.notused'
            Rename-Item -LiteralPath $regularBackup -NewName 'FanControl.LianLiPlugin.dll'
            Write-Host 'Original Fan Control plugin restored.'
        }
        Start-Process $fanExe -WorkingDirectory 'C:\Program Files (x86)\FanControl'
        Write-Host 'Fan Control restarted with existing curves.'
    }
    Stop-Transcript
}
Read-Host 'Capture finished. Tell Codex whether ON changed the fan lights. Press Enter to close'
