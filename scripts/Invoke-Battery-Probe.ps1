param(
    [ValidateSet('Inspect','Observe','Awake')][string]$Mode='Observe',
    [switch]$ConfirmHeadsetOn,
    [string]$ToolchainRoot='C:\Users\valerian\git-repos\OpenRGB-KFA2\RGB-investigation\toolchain'
)
$ErrorActionPreference='Stop'
if($Mode -eq 'Awake' -and -not $ConfirmHeadsetOn){throw 'Owner confirmation that the headset is awake is required'}
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$compilerBin=Join-Path $ToolchainRoot 'Tools\mingw1310_64\bin'
$env:PATH="$compilerBin;$env:PATH"
$buildRoot=Join-Path $repoRoot 'build-battery\probe'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$probe=Join-Path $buildRoot 'BatteryStatusProbe.exe'
& "$compilerBin\g++.exe" -std=c++17 -O2 "-I$repoRoot" "-I$repoRoot\dependencies\json" "$repoRoot\scripts\BatteryStatusProbe.cpp" "$repoRoot\BatteryStatus\AsusDeltaIIBattery.cpp" "$repoRoot\Controllers\AsusAuraUSBController\AsusDeltaIIController\AsusDeltaIIReceiver_Windows.cpp" "$repoRoot\BatteryStatus\WindowsHidTransfer.cpp" -lhid -lsetupapi -o $probe
if($LASTEXITCODE -ne 0){throw 'Battery probe build failed'}
$runId=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$stdout=Join-Path $buildRoot "$runId.json"
$stderr=Join-Path $buildRoot "$runId.stderr.log"
$arguments=switch($Mode){'Awake'{@('--awake-read','--confirm-headset-on')};'Inspect'{@('--check-interface')};default{@('--observe')}}
$process=Start-Process -FilePath $probe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
if(-not $process.WaitForExit(6000)){
    $process.Kill()
    $process.WaitForExit()
    throw 'Probe exceeded six seconds and was stopped; no automatic retry'
}
Get-Content -LiteralPath $stdout -Raw
if($process.ExitCode -ne 0){throw "Probe refused or failed; inspect $stdout and $stderr"}
