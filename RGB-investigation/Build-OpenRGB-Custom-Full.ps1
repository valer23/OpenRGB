$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$buildRoot = Join-Path $PSScriptRoot 'OpenRGB-Custom-full-build'
$compilerBin = Join-Path $PSScriptRoot 'toolchain\Tools\mingw1310_64\bin'
$qtBin = Join-Path $PSScriptRoot 'toolchain\6.8.3\mingw_64\bin'
$env:PATH = $compilerBin + ';' + $qtBin + ';' + $env:PATH
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$mbedArchive = (Join-Path $PSScriptRoot 'mbedtls-mingw\libmbedall.a').Replace('\','/')
Push-Location $buildRoot
try {
    & (Join-Path $qtBin 'qmake.exe') (Join-Path $repoRoot 'OpenRGB.pro') CONFIG-=debug_and_release CONFIG+=release CONFIG+=custom_full "KFA2_MBEDTLS_ARCHIVE=$mbedArchive" -after CONFIG-=precompile_header PRECOMPILED_HEADER= *> configure.log
    if($LASTEXITCODE -ne 0){throw 'Full configuration failed; see configure.log.'}
    & (Join-Path $compilerBin 'mingw32-make.exe') -j6 *> build.log
    if($LASTEXITCODE -ne 0){throw 'Full compilation failed; see build.log.'}
    & (Join-Path $qtBin 'windeployqt.exe') --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $buildRoot 'release\OpenRGB-Custom.exe') *> deploy.log
    if($LASTEXITCODE -ne 0){throw 'Runtime deployment failed; see deploy.log.'}
} finally {Pop-Location}
