param([switch]$IncludeFans)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$buildRoot = Join-Path $PSScriptRoot 'OpenRGB-KFA2-build'
$compilerBin = Join-Path $PSScriptRoot 'toolchain\Tools\mingw1310_64\bin'
$qtBin = Join-Path $PSScriptRoot 'toolchain\6.8.3\mingw_64\bin'
$env:PATH = $compilerBin + ';' + $qtBin + ';' + $env:PATH
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$python = 'C:\Users\valerian\AppData\Local\Programs\Python\Python312\python.exe'
& $python (Join-Path $PSScriptRoot 'build_mbedtls.py')
if ($LASTEXITCODE -ne 0) { throw 'Native mbedTLS compilation failed.' }
$mbedArchive = (Join-Path $PSScriptRoot 'mbedtls-mingw\libmbedall.a').Replace('\', '/')
$extraConfig = @()
if ($IncludeFans) { $extraConfig += 'CONFIG+=slinf_capture_test' }
Push-Location $buildRoot
try {
    & (Join-Path $qtBin 'qmake.exe') (Join-Path $repoRoot 'OpenRGB.pro') CONFIG-=debug_and_release CONFIG+=release CONFIG+=kfa2_capture_test "KFA2_MBEDTLS_ARCHIVE=$mbedArchive" @extraConfig *> configure.log
    if ($LASTEXITCODE -ne 0) { throw 'Configuration failed; see configure.log.' }
    # qmake does not invalidate MinGW PCH files when configuration defines change.
    foreach ($pchName in @('c', 'c++')) {
        $pchFile = Join-Path $buildRoot ('_intermediate_release\.obj\precompiled_header.h.gch\' + $pchName)
        if (Test-Path -LiteralPath $pchFile -PathType Leaf) { Remove-Item -LiteralPath $pchFile }
    }
    & (Join-Path $compilerBin 'mingw32-make.exe') -j6 *> build.log
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed; see build.log.' }
    & (Join-Path $qtBin 'windeployqt.exe') --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $buildRoot 'release\OpenRGB-KFA2-test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt runtime deployment failed.' }
} finally { Pop-Location }
