param([string]$ToolchainRoot='C:\Users\valerian\git-repos\OpenRGB-KFA2\RGB-investigation\toolchain',[int]$Jobs=4)
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$qtBin=Join-Path $ToolchainRoot '6.8.3\mingw_64\bin'
$compilerBin=Join-Path $ToolchainRoot 'Tools\mingw1310_64\bin'
$env:PATH="$compilerBin;$qtBin;$env:PATH"
$buildRoot=Join-Path $repoRoot 'build-battery\core'
$mbedLibrary='C:/Users/valerian/git-repos/OpenRGB-KFA2/RGB-investigation/mbedtls-mingw/libmbedall.a'
if(-not(Test-Path -LiteralPath $mbedLibrary)){throw 'Existing MinGW mbedTLS library is required'}
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
# Reuse the existing personal build's MinGW portability adaptations in generated
# build sources. Keep vendored code and upstream-facing controller changes untouched.
$timeSource=Get-Content "$repoRoot\dependencies\hueplusplus-1.2.0\src\TimePattern.cpp" -Raw
$timeSource=$timeSource.Replace('#ifdef _MSC_VER','#ifdef _WIN32')
[System.IO.File]::WriteAllText("$buildRoot\compat-TimePattern.cpp",$timeSource)
$wmiSource=Get-Content "$repoRoot\wmi\wmi.cpp" -Raw
$wmiSource=$wmiSource.Replace('bstr_t("WQL"),','bstr_t(L"WQL"),').Replace('bstr_t(queryStr.c_str()),','bstr_t(utf8_decode(queryStr).c_str()),')
[System.IO.File]::WriteAllText("$buildRoot\compat-wmi.cpp",$wmiSource)
$buildQmake=$buildRoot.Replace('\','/')
$repoQmake=$repoRoot.Replace('\','/')
Push-Location $buildRoot
try {
    & "$qtBin\qmake.exe" "$repoRoot\OpenRGB.pro" 'CONFIG-=debug_and_release' 'CONFIG+=release' 'QMAKE_TARGET.arch=x86_64' -after 'CONFIG-=precompile_header' 'PRECOMPILED_HEADER=' 'QMAKE_CXXFLAGS -= /utf-8' 'LIBS -= -lmbedcrypto -lmbedtls -lmbedx509' "LIBS += $mbedLibrary -lole32 -loleaut32 -luuid -lwbemuuid -lsetupapi -lwinhttp -lws2_32 -liphlpapi" 'SOURCES -= dependencies/hueplusplus-1.2.0/src/TimePattern.cpp' "SOURCES += $buildQmake/compat-TimePattern.cpp" 'SOURCES -= wmi/wmi.cpp' "SOURCES += $buildQmake/compat-wmi.cpp" "INCLUDEPATH += $repoQmake/wmi"
    if($LASTEXITCODE -ne 0){throw 'Core configuration failed'}
    & "$compilerBin\mingw32-make.exe" "-j$Jobs"
    if($LASTEXITCODE -ne 0){throw 'Core build failed'}
} finally {Pop-Location}
