param([string]$ToolchainRoot='C:\Users\valerian\git-repos\OpenRGB-KFA2\RGB-investigation\toolchain')
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$compilerBin=Join-Path $ToolchainRoot 'Tools\mingw1310_64\bin'
$qtBin=Join-Path $ToolchainRoot '6.8.3\mingw_64\bin'
$env:PATH="$compilerBin;$qtBin;$env:PATH"
$buildRoot=Join-Path $repoRoot 'build-battery\tests'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
& (Join-Path $compilerBin 'g++.exe') -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' tests/battery-status/cache_test.cpp BatteryStatus/BatteryStatusManager.cpp -o "$buildRoot\cache_test.exe"
if($LASTEXITCODE -ne 0){throw 'Cache test compilation failed'}
& "$buildRoot\cache_test.exe"
if($LASTEXITCODE -ne 0){throw 'Cache tests failed'}

$qtRoot=Split-Path $qtBin -Parent
$includes=@('-I.','-Idependencies/json','-IRGBController',"-I$qtRoot\include","-I$qtRoot\include\QtCore","-I$qtRoot\include\QtGui","-I$qtRoot\include\QtWidgets")
$libs=@("-L$qtRoot\lib",'-lQt6Widgets','-lQt6Gui','-lQt6Core')
& "$compilerBin\g++.exe" -std=c++17 @includes tests/battery-status/presentation_test.cpp @libs -o "$buildRoot\presentation_test.exe"
if($LASTEXITCODE -ne 0){throw 'Presentation test compilation failed'}
& "$buildRoot\presentation_test.exe"
if($LASTEXITCODE -ne 0){throw 'Presentation tests failed'}
& "$compilerBin\g++.exe" -std=c++17 -frtti @includes tests/battery-status/plugin_test.cpp BatteryStatus/BatteryStatusManager.cpp @libs -o "$buildRoot\plugin_test.exe"
if($LASTEXITCODE -ne 0){throw 'Plugin lifecycle test compilation failed'}
$env:QT_QPA_PLATFORM='offscreen'
& "$buildRoot\plugin_test.exe" "$repoRoot\build-battery\plugin\OpenRGBBatteryStatusPlugin.dll"
if($LASTEXITCODE -ne 0){throw 'Plugin lifecycle or cross-DLL RTTI tests failed'}
