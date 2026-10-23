param([string]$Compiler = 'g++')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$testOutput = Join-Path ([IO.Path]::GetTempPath()) ('openrgb-driver-tests-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testOutput | Out-Null
Push-Location $repoRoot
try {
    $common = @('-std=c++17', '-Itests/driver-regressions/stubs', '-I.', '-Idependencies/NVFC', '-Idependencies/hidapi-win/include')
    if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'kfa2_driver_test.cpp')) {
        $exe = Join-Path $testOutput 'kfa2.exe'
        & $Compiler @common -Ii2c_smbus -IControllers/GalaxGPUController/GalaxGPUv1Controller tests/driver-regressions/kfa2_driver_test.cpp Controllers/GalaxGPUController/GalaxGPUv1Controller/GalaxGPUv1Controller.cpp Controllers/GalaxGPUController/GalaxGPUv1Controller/RGBController_GalaxGPUv1.cpp i2c_smbus/i2c_smbus.cpp -o $exe
        if($LASTEXITCODE -ne 0) {throw 'GPU regression test compilation failed.'}
        & $exe
        if($LASTEXITCODE -ne 0) {throw 'GPU regression tests failed.'}
    }
    if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'nvapi_wrapper_test.cpp')) {
        $exe = Join-Path $testOutput 'nvapi.exe'
        & $Compiler @common tests/driver-regressions/nvapi_wrapper_test.cpp -o $exe
        if($LASTEXITCODE -ne 0) {throw 'NVAPI wrapper test compilation failed.'}
        & $exe
        if($LASTEXITCODE -ne 0) {throw 'NVAPI wrapper tests failed.'}
    }
    if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'lianli_lighting_test.cpp')) {
        $exe = Join-Path $testOutput 'lianli.exe'
        & $Compiler -Idependencies/hidapi-hotplug-win/include @common -IControllers/LianLiController/LianLiUniHubSLInfinityController tests/driver-regressions/lianli_lighting_test.cpp Controllers/LianLiController/LianLiUniHubSLInfinityController/LianLiUniHubSLInfinityController.cpp Controllers/LianLiController/LianLiUniHubSLInfinityController/RGBController_LianLiUniHubSLInfinity.cpp -o $exe
        if($LASTEXITCODE -ne 0) {throw 'Fan regression test compilation failed.'}
        & $exe
        if($LASTEXITCODE -ne 0) {throw 'Fan regression tests failed.'}
    }
    if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'asus_neo_test.cpp')) {
        $exe = Join-Path $testOutput 'asus-neo.exe'
        & $Compiler -Idependencies/hidapi-hotplug-win/include @common -IControllers/AsusAuraUSBController/AsusAuraNeoController tests/driver-regressions/asus_neo_test.cpp Controllers/AsusAuraUSBController/AsusAuraNeoController/RGBController_AsusAuraNeo_Windows.cpp -o $exe
        if($LASTEXITCODE -ne 0) {throw 'ASUS NEO regression test compilation failed.'}
        & $exe
        if($LASTEXITCODE -ne 0) {throw 'ASUS NEO regression tests failed.'}
    }
} finally {Pop-Location}
