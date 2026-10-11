$ErrorActionPreference = 'Stop'
$testExe = Join-Path $PSScriptRoot 'OpenRGB-KFA2-build\release\OpenRGB-Custom.exe'
$settingsRoot = Join-Path $PSScriptRoot 'OpenRGB-KFA2-settings'
try {
    if (-not (Test-Path -LiteralPath $testExe)) { throw 'The test build is not ready yet.' }
    if (Get-Process 'OpenRGB','OpenRGB-KFA2-test','OpenRGB-Custom','Xtreme Tuner' -ErrorAction SilentlyContinue) {
        throw 'Exit OpenRGB and Xtreme Tuner from their tray icons before opening the test build.'
    }
    if (-not (Get-Process FanControl -ErrorAction SilentlyContinue)) { throw 'Keep Fan Control running.' }
    Write-Host 'Opening the GPU and SL-Infinity lighting test build with separate settings.' -ForegroundColor Cyan
    & $testExe --config $settingsRoot --gui
} catch {
    Write-Host $_ -ForegroundColor Red
    Read-Host 'Press Enter to close'
}
