$ErrorActionPreference = 'Stop'
$fanExe = 'C:\Program Files (x86)\FanControl\FanControl.exe'
$regular = 'C:\Program Files (x86)\FanControl\Plugins\FanControl.LianLiPlugin.dll'
$regularBackup = 'C:\Program Files (x86)\FanControl\Plugins\FanControl.LianLiPlugin.dll.before-ARGB-test'
$argbInactive = 'C:\Program Files (x86)\FanControl\Plugins\FanControl.LianLiPlugin.ARGB.dll.notused'
$argbActive = 'C:\Program Files (x86)\FanControl\Plugins\FanControl.LianLiPlugin.ARGB.dll'
$fanConfig = 'C:\Program Files (x86)\FanControl\Configurations\ValFans.json'
$testRoot = $PSScriptRoot
$lightingWasRunning = $false
$pluginSwitched = $false
$fanStarted = $false
Start-Transcript -Path (Join-Path $testRoot 'Fan-ARGB-test-result.txt') -Force
try {
    Write-Host 'Enable Lian Li motherboard lighting mode' -ForegroundColor Cyan
    Write-Host 'Close Armoury Crate and OpenRGB. Keep L-Connect closed.'
    Write-Host 'Exit Fan Control using its tray menu, then return here.'
    Read-Host 'Press Enter after Fan Control has exited'
    if (Get-Process -Name FanControl -ErrorAction SilentlyContinue) {
        throw 'Fan Control is still running. No plugin files were changed. Exit it and run the test again.'
    }
    foreach ($requiredFile in @($fanExe, $regular, $argbInactive, $fanConfig)) {
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) { throw "Missing required file: $requiredFile" }
    }
    if ((Test-Path -LiteralPath $regularBackup) -or (Test-Path -LiteralPath $argbActive)) {
        throw 'Plugin destination already exists. No plugin files were changed.'
    }
    $configBackup = Join-Path $testRoot ('ValFans-before-ARGB-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.json')
    Copy-Item -LiteralPath $fanConfig -Destination $configBackup
    Rename-Item -LiteralPath $regular -NewName 'FanControl.LianLiPlugin.dll.before-ARGB-test'
    try {
        Rename-Item -LiteralPath $argbInactive -NewName 'FanControl.LianLiPlugin.ARGB.dll'
    } catch {
        Rename-Item -LiteralPath $regularBackup -NewName 'FanControl.LianLiPlugin.dll'
        throw
    }
    $pluginSwitched = $true
    Start-Process -FilePath $fanExe -WorkingDirectory 'C:\Program Files (x86)\FanControl'
    $fanStarted = $true
    Write-Host 'ARGB plugin activated. Fan Control has been started.' -ForegroundColor Green
    Write-Host 'Check that your existing fan curves and RPM readings are present.'
    Read-Host 'Press Enter when Fan Control is ready and you can watch the lights'
    if (-not (Get-Process -Name FanControl -ErrorAction SilentlyContinue)) { throw 'Fan Control is not running. Skipping lighting replay.' }
    $lightingWasRunning = (Get-Service LightingService).Status -eq 'Running'
    if ($lightingWasRunning) {
        Stop-Service LightingService
        (Get-Service LightingService).WaitForStatus('Stopped', [TimeSpan]::FromSeconds(15))
    }
    & 'C:\Users\valerian\AppData\Local\Programs\Python\Python312\python.exe' (Join-Path $testRoot 'aura_hid.py') --replay
    if ($LASTEXITCODE -ne 0) { throw "Replay exited with code $LASTEXITCODE" }
} catch {
    Write-Host $_ -ForegroundColor Red
    if ($pluginSwitched -and -not (Get-Process -Name FanControl -ErrorAction SilentlyContinue)) {
        Rename-Item -LiteralPath $argbActive -NewName 'FanControl.LianLiPlugin.ARGB.dll.notused'
        Rename-Item -LiteralPath $regularBackup -NewName 'FanControl.LianLiPlugin.dll'
        Start-Process -FilePath $fanExe -WorkingDirectory 'C:\Program Files (x86)\FanControl'
        Write-Host 'Original plugin restored and Fan Control restarted.'
    }
} finally {
    if ((Test-Path -LiteralPath $fanExe) -and -not (Get-Process -Name FanControl -ErrorAction SilentlyContinue)) {
        Start-Process -FilePath $fanExe -WorkingDirectory 'C:\Program Files (x86)\FanControl'
        Write-Host 'Fan Control restarted.'
    }
    if ($lightingWasRunning) {
        Start-Service LightingService
        Write-Host 'ASUS lighting service restarted.'
    }
    Stop-Transcript
}
Read-Host 'Tell Codex whether the fans followed red/green/blue/red. Press Enter to close'
