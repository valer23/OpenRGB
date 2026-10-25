param([string]$ToolchainRoot='C:\Users\valerian\git-repos\OpenRGB-KFA2\RGB-investigation\toolchain', [string]$ReceiverControlPath)
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$compilerBin=Join-Path $ToolchainRoot 'Tools\mingw1310_64\bin'
$env:PATH="$compilerBin;$env:PATH"
$build=Join-Path $repoRoot 'build-battery\tests'
New-Item -ItemType Directory -Path $build -Force | Out-Null
Push-Location $repoRoot
try {
    & "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-I.' tests/corsair-recovery/policy_test.cpp -o "$build\corsair_recovery_policy_test.exe"
    if($LASTEXITCODE -ne 0){throw 'Recovery policy compilation failed'}
    & "$build\corsair_recovery_policy_test.exe"
    if($LASTEXITCODE -ne 0){throw 'Recovery policy regression failed'}
    & "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra '-Itests/corsair-recovery/stubs' '-I.' tests/corsair-recovery/windows_shadow_test.cpp Controllers/CorsairPeripheralV2Controller/CorsairStartupRecovery_Windows.cpp -lsetupapi -luser32 -o "$build\corsair_windows_shadow_test.exe"
    if($LASTEXITCODE -ne 0){throw 'Windows observer compilation failed'}
    if($ReceiverControlPath){
        & "$build\corsair_windows_shadow_test.exe" --registrations $ReceiverControlPath
        if($LASTEXITCODE -ne 0){throw 'Windows registration ownership regression failed'}
    }
    # Existing real-driver/packet tests, with only its clock injected. No detector,
    # observer or Windows/HID device implementation is linked into these tests.
    $source=Get-Content Controllers/CorsairPeripheralV2Controller/CorsairPeripheralV2Controller.cpp -Raw
    $pattern='(?s)static unsigned long long CorsairMouseTime\(\)\s*\{.*?\n\}'
    if(([regex]::Matches($source,$pattern)).Count -ne 1){throw 'Clock seam changed'}
    $source="#include `"MouseNativeClock.h`"`n"+[regex]::Replace($source,$pattern,'static unsigned long long CorsairMouseTime() { return mouse_test_now; }')
    [IO.File]::WriteAllText("$build\CorsairRecoveryUnderTest.cpp",$source)
    $includes=@('-Itests/battery-status/stubs','-IControllers/CorsairPeripheralV2Controller','-Idependencies/hidapi-hotplug-win/include','-I.','-Idependencies/json')
    foreach($test in @('corsair_driver','corsair_packet_trace')){
        & "$compilerBin\g++.exe" -std=c++17 -Wall -Wextra @includes "tests/battery-status/${test}_test.cpp" "$build\CorsairRecoveryUnderTest.cpp" BatteryStatus/BatteryStatusManager.cpp -o "$build\recovery_${test}_test.exe"
        if($LASTEXITCODE -ne 0){throw "$test compilation failed"}
        & "$build\recovery_${test}_test.exe"
        if($LASTEXITCODE -ne 0){throw "$test failed"}
    }
} finally {Pop-Location}
