param(
    [string]$AppRoot = 'C:\Users\valerian\Apps\OpenRGB-Custom',
    [string]$Python = 'C:\Users\valerian\AppData\Local\Programs\Python\Python312\python.exe'
)
$ErrorActionPreference = 'Stop'
if(Get-Process OpenRGB-Custom -ErrorAction SilentlyContinue){throw 'Exit OpenRGB before installing effects'}
$buildFiles = Join-Path $PSScriptRoot '.build\release-build\release'
$settingsRoot = Join-Path $AppRoot 'Settings'
$pluginRoot = Join-Path $settingsRoot 'plugins'
$generatedProfiles = Join-Path $PSScriptRoot 'profiles'
if(-not(Test-Path -LiteralPath (Join-Path $buildFiles 'OpenRGBEffectsPlugin.dll'))){throw 'Build the Effects Plugin first'}
New-Item -ItemType Directory -Path $pluginRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $buildFiles 'OpenRGBEffectsPlugin.dll') -Destination $pluginRoot -Force
foreach($runtime in @('Qt6Core5Compat.dll','Qt6OpenGL.dll')){
    Copy-Item -LiteralPath (Join-Path $buildFiles $runtime) -Destination $AppRoot -Force
}
& $Python (Join-Path $PSScriptRoot 'make_profiles.py') (Join-Path $settingsRoot 'profiles\ValWhite.json') $generatedProfiles
if($LASTEXITCODE -ne 0){throw 'Effects profile creation failed'}
foreach($profile in @('Rainbow Wave','Brimstone')){
    $destination = Join-Path $settingsRoot ('profiles\' + $profile + '.json')
    if(Test-Path -LiteralPath $destination){
        $backup = $destination + '.before-effects-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.bak'
        Copy-Item -LiteralPath $destination -Destination $backup
    }
    Copy-Item -LiteralPath (Join-Path $generatedProfiles ($profile + '.json')) -Destination $destination -Force
}
$pluginSettings = Join-Path $pluginRoot 'settings'
New-Item -ItemType Directory -Path (Join-Path $pluginSettings 'effect-profiles') -Force | Out-Null
$effectSettings = Join-Path $pluginSettings 'EffectSettings.json'
if(-not(Test-Path -LiteralPath $effectSettings)){
    [IO.File]::WriteAllText($effectSettings,'{"fps":10,"brightness":100,"hide_unsupported":true,"prefer_random":false,"startup_timeout":2000}',[Text.UTF8Encoding]::new($false))
}
Write-Host 'Effects installed. Startup profile remains ValWhite.'
