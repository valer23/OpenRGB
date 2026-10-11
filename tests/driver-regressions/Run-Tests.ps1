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
        & $Compiler @common -IControllers/GalaxGPUController tests/driver-regressions/kfa2_driver_test.cpp Controllers/GalaxGPUController/RGBController_KFA2EX_Windows.cpp -o $exe
        if($LASTEXITCODE -ne 0) {throw 'GPU regression test compilation failed.'}
        & $exe
        if($LASTEXITCODE -ne 0) {throw 'GPU regression tests failed.'}
    }
    if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'lianli_lighting_test.cpp')) {
        $exe = Join-Path $testOutput 'lianli.exe'
        & $Compiler @common -IControllers/LianLiController/LianLiUniHubSLInfinityController tests/driver-regressions/lianli_lighting_test.cpp Controllers/LianLiController/LianLiUniHubSLInfinityController/LianLiUniHubSLInfinityController.cpp Controllers/LianLiController/LianLiUniHubSLInfinityController/RGBController_LianLiUniHubSLInfinity.cpp -o $exe
        if($LASTEXITCODE -ne 0) {throw 'Fan regression test compilation failed.'}
        & $exe
        if($LASTEXITCODE -ne 0) {throw 'Fan regression tests failed.'}
    }
} finally {Pop-Location}
