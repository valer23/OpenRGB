param([string]$ToolchainRoot='C:\Users\valerian\git-repos\OpenRGB-KFA2\RGB-investigation\toolchain')
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$compilerBin=Join-Path $ToolchainRoot 'Tools\mingw1310_64\bin'
$env:PATH="$compilerBin;$env:PATH"
$buildRoot=Join-Path $repoRoot 'build-battery\tests'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
Push-Location $repoRoot
try {
    foreach($name in @('protocol','receiver','battery_receiver','frame_queue','audio_gate','direct_commands','control')) {
        $sources=@("tests/asus-delta-ii/${name}_test.cpp")
        if($name -eq 'battery_receiver') { $sources += 'BatteryStatus/AsusDeltaIIBattery.cpp' }
        & "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' @sources -o "$buildRoot\delta_${name}_test.exe"
        if($LASTEXITCODE -ne 0) { throw "Delta II $name test compilation failed" }
        & "$buildRoot\delta_${name}_test.exe"
        if($LASTEXITCODE -ne 0) { throw "Delta II $name test failed" }
    }
} finally { Pop-Location }
