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

# Compile the actual controller, replacing only its clock for deterministic transport tests.
$controllerSource=Get-Content "$repoRoot\Controllers\CorsairPeripheralV2Controller\CorsairPeripheralV2Controller.cpp" -Raw
$clockPattern='(?s)static unsigned long long CorsairMouseTime\(\)\s*\{.*?\n\}'
if(([regex]::Matches($controllerSource,$clockPattern)).Count -ne 1){throw 'Controller clock test seam changed'}
$controllerSource="#include `"MouseNativeClock.h`"`n"+[regex]::Replace($controllerSource,$clockPattern,'static unsigned long long CorsairMouseTime() { return mouse_test_now; }')
[System.IO.File]::WriteAllText("$buildRoot\CorsairUnderTest.cpp",$controllerSource)
$driverIncludes=@('-Itests/battery-status/stubs','-IControllers/CorsairPeripheralV2Controller','-Idependencies/hidapi-hotplug-win/include','-I.','-Idependencies/json')
& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra @driverIncludes tests/battery-status/corsair_driver_test.cpp "$buildRoot\CorsairUnderTest.cpp" BatteryStatus/BatteryStatusManager.cpp -o "$buildRoot\corsair_driver_test.exe"
if($LASTEXITCODE -ne 0){throw 'Actual Corsair driver test compilation failed'}
& "$buildRoot\corsair_driver_test.exe"
if($LASTEXITCODE -ne 0){throw 'Actual Corsair driver tests failed'}

& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' tests/battery-status/protocol_test.cpp -o "$buildRoot\protocol_test.exe"
if($LASTEXITCODE -ne 0){throw 'Protocol test compilation failed'}
& "$buildRoot\protocol_test.exe"
if($LASTEXITCODE -ne 0){throw 'Protocol tests failed'}

& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' tests/battery-status/validation_gate_test.cpp BatteryStatus/AsusDeltaIIBattery.cpp -lhid -lsetupapi -o "$buildRoot\validation_gate_test.exe"
if($LASTEXITCODE -ne 0){throw 'Validation gate test compilation failed'}
& "$buildRoot\validation_gate_test.exe"
if($LASTEXITCODE -ne 0){throw 'Validation gate test failed'}
& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' scripts/BatteryStatusProbe.cpp BatteryStatus/AsusDeltaIIBattery.cpp -lhid -lsetupapi -o "$buildRoot\BatteryStatusProbe.exe"
if($LASTEXITCODE -ne 0){throw 'Validation probe compilation failed'}
