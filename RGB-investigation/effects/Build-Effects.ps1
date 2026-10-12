param([string]$CompilerRoot = '')
$ErrorActionPreference = 'Stop'
$investigationRoot = Split-Path $PSScriptRoot -Parent
if(-not $CompilerRoot){$CompilerRoot = Join-Path $investigationRoot 'toolchain'}
$qtBin = Join-Path $CompilerRoot '6.8.3\mingw_64\bin'
$compilerBin = Join-Path $CompilerRoot 'Tools\mingw1310_64\bin'
$env:PATH = $compilerBin + ';' + $qtBin + ';' + $env:PATH
$source = Join-Path $PSScriptRoot '.build\source'
$build = Join-Path $PSScriptRoot '.build\release-build'
$revision = '8288ef45eb27d98a237b11ae21fa76be17978c2a'
if(-not(Test-Path -LiteralPath (Join-Path $source '.git'))){
    New-Item -ItemType Directory -Path (Split-Path $source -Parent) -Force | Out-Null
    & git clone --depth 1 https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin.git $source
    if($LASTEXITCODE -ne 0){throw 'Effects source clone failed'}
    & git -C $source fetch --depth 1 origin $revision
    if($LASTEXITCODE -ne 0){throw 'Pinned effects source fetch failed'}
    & git -C $source checkout --detach $revision
    if($LASTEXITCODE -ne 0){throw 'Pinned effects source checkout failed'}
}
if((& git -C $source rev-parse HEAD) -ne $revision){throw 'Build source is not the pinned effects revision'}
& git -C $source submodule update --init --depth 1
if($LASTEXITCODE -ne 0){throw 'Effects dependency checkout failed'}
$patch = Join-Path $PSScriptRoot 'compatibility.patch'
& git -C $source apply --check $patch 2>$null
if($LASTEXITCODE -eq 0){
    & git -C $source apply $patch
    if($LASTEXITCODE -ne 0){throw 'Effects compatibility patch failed'}
}else{
    & git -C $source apply --reverse --check $patch 2>$null
    if($LASTEXITCODE -ne 0){throw 'Effects source has unrelated changes; preserving it'}
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'plugin\CompatibleDirectMode.h') -Destination $source -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'plugin\EffectTimerResolution.h') -Destination $source -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'plugin\EffectFrameBatch.h') -Destination $source -Force
New-Item -ItemType Directory -Path (Join-Path $source 'Effects\Brimstone') -Force | Out-Null
Copy-Item -Path (Join-Path $PSScriptRoot 'plugin\Brimstone\*') -Destination (Join-Path $source 'Effects\Brimstone') -Force
New-Item -ItemType Directory -Path (Join-Path $source 'Effects\IcueLayers') -Force | Out-Null
Copy-Item -Path (Join-Path $PSScriptRoot 'plugin\IcueLayers\*') -Destination (Join-Path $source 'Effects\IcueLayers') -Force
New-Item -ItemType Directory -Path (Join-Path $source 'Effects\KeyRipple') -Force | Out-Null
Copy-Item -Path (Join-Path $PSScriptRoot 'plugin\KeyRipple\*') -Destination (Join-Path $source 'Effects\KeyRipple') -Force
New-Item -ItemType Directory -Path $build -Force | Out-Null
Push-Location $build
try {
    & (Join-Path $qtBin 'qmake.exe') (Join-Path $source 'OpenRGBEffectsPlugin.pro') CONFIG-=debug_and_release CONFIG+=release *> configure.log
    if($LASTEXITCODE -ne 0){throw 'Effects configuration failed; Qt5Compat 6.8.3 must be installed'}
    & (Join-Path $compilerBin 'mingw32-make.exe') -j6 *> build.log
    if($LASTEXITCODE -ne 0){throw 'Effects compilation failed; see build.log'}
    & (Join-Path $qtBin 'windeployqt.exe') --release --no-translations --no-system-d3d-compiler --no-opengl-sw release\OpenRGBEffectsPlugin.dll *> deploy.log
    if($LASTEXITCODE -ne 0){throw 'Effects runtime deployment failed'}
}finally{Pop-Location}
