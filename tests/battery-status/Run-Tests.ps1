param([string]$ToolchainRoot='C:\Users\valerian\git-repos\OpenRGB-KFA2\RGB-investigation\toolchain')
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'handle_result_test.ps1')
& (Join-Path $PSScriptRoot 'service_trial_test.ps1')
& (Join-Path $PSScriptRoot 'sleep_wrapper_test.ps1')
& (Join-Path $PSScriptRoot 'trial_packaging_test.ps1')
& (Join-Path $PSScriptRoot 'gpu_identity_migration_test.ps1')
& (Join-Path $PSScriptRoot 'profile_schema_migration_test.ps1')
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$compilerBin=Join-Path $ToolchainRoot 'Tools\mingw1310_64\bin'
$qtBin=Join-Path $ToolchainRoot '6.8.3\mingw_64\bin'
$env:PATH="$compilerBin;$qtBin;$env:PATH"
$buildRoot=Join-Path $repoRoot 'build-battery\tests'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
Push-Location $repoRoot
try {
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
& "$buildRoot\plugin_test.exe" "$repoRoot\build-battery\plugin\OpenRGBBatteryStatusPlugin.dll" "C:\Users\valerian\Apps\OpenRGB-Custom\Settings\plugins\OpenRGBEffectsPlugin.dll"
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
& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra @driverIncludes tests/battery-status/corsair_packet_trace_test.cpp "$buildRoot\CorsairUnderTest.cpp" BatteryStatus/BatteryStatusManager.cpp -o "$buildRoot\corsair_packet_trace_test.exe"
if($LASTEXITCODE -ne 0){throw 'Installed Corsair packet comparison compilation failed'}
& "$buildRoot\corsair_packet_trace_test.exe"
if($LASTEXITCODE -ne 0){throw 'Battery hooks changed installed mouse control packets'}


& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' tests/battery-status/asus_monitor_test.cpp BatteryStatus/BatteryStatusManager.cpp BatteryStatus/AsusBatteryMonitor.cpp -o "$buildRoot\asus_monitor_test.exe"
if($LASTEXITCODE -ne 0){throw 'ASUS monitor test compilation failed'}
& "$buildRoot\asus_monitor_test.exe"
if($LASTEXITCODE -ne 0){throw 'ASUS monitor tests failed'}
# Pause the actual monitor between predicate evaluation and wait to force the
# shutdown interleaving that ordinary lifecycle tests cannot guarantee.
$monitorSource=Get-Content "$repoRoot\BatteryStatus\AsusBatteryMonitor.cpp" -Raw
$waitPredicate='[this] { return stop.load(); }'
if(([regex]::Matches($monitorSource,[regex]::Escape($waitPredicate))).Count -ne 1){throw 'ASUS wait test seam changed'}
$monitorSource='#include "AsusWaitHooks.h"'+"`n"+$monitorSource.Replace($waitPredicate,'[this] { const bool stopping = stop.load(); AsusPredicateTestHook(stopping); return stopping; }').Replace('changed.notify_all();','changed.notify_all(); AsusNotifyTestHook();')
[System.IO.File]::WriteAllText("$buildRoot\AsusMonitorUnderTest.cpp",$monitorSource)
& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' '-IBatteryStatus' '-Itests/battery-status' tests/battery-status/asus_stop_race_test.cpp "$buildRoot\AsusMonitorUnderTest.cpp" BatteryStatus/BatteryStatusManager.cpp -o "$buildRoot\asus_stop_race_test.exe"
if($LASTEXITCODE -ne 0){throw 'ASUS stop interleaving test compilation failed'}
& "$buildRoot\asus_stop_race_test.exe"
if($LASTEXITCODE -ne 0){throw 'ASUS stop interleaving regression failed'}
& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' tests/battery-status/protocol_test.cpp -o "$buildRoot\protocol_test.exe"
if($LASTEXITCODE -ne 0){throw 'Protocol test compilation failed'}
& "$buildRoot\protocol_test.exe"
if($LASTEXITCODE -ne 0){throw 'Protocol tests failed'}
& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' tests/battery-status/asus_log_test.cpp BatteryStatus/AsusBatteryLog.cpp BatteryStatus/WindowsHidTransfer.cpp -ladvapi32 -o "$buildRoot\asus_log_test.exe"
if($LASTEXITCODE -ne 0){throw 'ASUS log reader compilation failed'}
& "$buildRoot\asus_log_test.exe"
if($LASTEXITCODE -ne 0){throw 'ASUS log reader tests failed'}


& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' tests/battery-status/windows_hid_transfer_test.cpp BatteryStatus/WindowsHidTransfer.cpp -o "$buildRoot\windows_hid_transfer_test.exe"
if($LASTEXITCODE -ne 0){throw 'Windows HID transport test compilation failed'}
& "$buildRoot\windows_hid_transfer_test.exe"
if($LASTEXITCODE -ne 0){throw 'Windows HID transport tests failed'}
& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' tests/battery-status/monitoring_service_test.cpp BatteryStatus/BatteryStatusManager.cpp BatteryStatus/AsusBatteryMonitor.cpp BatteryStatus/BatteryMonitoringService.cpp BatteryStatus/AsusBatteryLog.cpp BatteryStatus/AsusDeltaIIBattery.cpp Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIReceiver_Windows.cpp BatteryStatus/WindowsHidTransfer.cpp -lhid -lsetupapi -ladvapi32 -o "$buildRoot\monitoring_service_test.exe"
if($LASTEXITCODE -ne 0){throw 'Core monitoring service test compilation failed'}
& "$buildRoot\monitoring_service_test.exe"
if($LASTEXITCODE -ne 0){throw 'Core monitoring service tests failed'}
& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' tests/battery-status/validation_gate_test.cpp BatteryStatus/AsusDeltaIIBattery.cpp Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIReceiver_Windows.cpp BatteryStatus/WindowsHidTransfer.cpp -lhid -lsetupapi -o "$buildRoot\validation_gate_test.exe"
if($LASTEXITCODE -ne 0){throw 'Validation gate test compilation failed'}
& "$buildRoot\validation_gate_test.exe"
if($LASTEXITCODE -ne 0){throw 'Validation gate test failed'}
& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' '-Idependencies/json' scripts/BatteryStatusProbe.cpp BatteryStatus/AsusDeltaIIBattery.cpp Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIReceiver_Windows.cpp BatteryStatus/WindowsHidTransfer.cpp -lhid -lsetupapi -o "$buildRoot\BatteryStatusProbe.exe"
if($LASTEXITCODE -ne 0){throw 'Validation probe compilation failed'}

& "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra @driverIncludes tests/battery-status/review_safety_test.cpp "$buildRoot\CorsairUnderTest.cpp" BatteryStatus/BatteryStatusManager.cpp -o "$buildRoot\review_safety_test.exe"
if($LASTEXITCODE -ne 0){throw 'Review regression compilation failed'}
foreach($case in @('state','late')) {
    & "$buildRoot\review_safety_test.exe" $case
    if($LASTEXITCODE -ne 0){throw "Review regression $case failed"}
}

} finally { Pop-Location }
