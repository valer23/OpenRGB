param([string]$ToolchainRoot='C:\Users\valerian\git-repos\OpenRGB-KFA2\RGB-investigation\toolchain')
$ErrorActionPreference='Stop'
$qtBin=Join-Path $ToolchainRoot '6.8.3\mingw_64\bin'
$compilerBin=Join-Path $ToolchainRoot 'Tools\mingw1310_64\bin'
$env:PATH="$compilerBin;$qtBin;$env:PATH"
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildRoot=Join-Path $repoRoot 'build-battery\plugin'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
Push-Location $buildRoot
try {
    & "$qtBin\qmake.exe" (Join-Path $PSScriptRoot 'BatteryStatus.pro')
    if($LASTEXITCODE -ne 0){throw 'Battery plugin configuration failed'}
    & "$compilerBin\mingw32-make.exe" -j4
    if($LASTEXITCODE -ne 0){throw 'Battery plugin build failed'}
} finally {Pop-Location}
