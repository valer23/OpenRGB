param([string]$InstalledRoot='C:\Users\valerian\Apps\OpenRGB-Custom')
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$trialRoot=Join-Path $repoRoot 'build-battery\trial'
$coreExe=Join-Path $repoRoot 'build-battery\core\release\OpenRGB.exe'
$pluginDll=Join-Path $repoRoot 'build-battery\plugin\OpenRGBBatteryStatusPlugin.dll'
foreach($file in @($coreExe,$pluginDll,(Join-Path $InstalledRoot 'Settings\OpenRGB.json'))){
    if(-not(Test-Path -LiteralPath $file)){throw "Required trial input missing: $file"}
}
if(Test-Path -LiteralPath $trialRoot){throw 'Trial folder already exists; preserve it and select a fresh trial folder before preparing again'}
New-Item -ItemType Directory -Path "$trialRoot\Settings\plugins","$trialRoot\Settings\profiles" -Force | Out-Null
Get-ChildItem -LiteralPath $InstalledRoot -File | Where-Object { $_.Extension -in '.dll','.bin' } | Copy-Item -Destination $trialRoot
foreach($directory in @('generic','iconengines','imageformats','networkinformation','platforms','styles','tls')){
    $source=Join-Path $InstalledRoot $directory
    if(Test-Path -LiteralPath $source){Copy-Item -LiteralPath $source -Destination $trialRoot -Recurse}
}
Copy-Item -LiteralPath $coreExe -Destination "$trialRoot\OpenRGB-Battery-Trial.exe"
Copy-Item -LiteralPath $pluginDll -Destination "$trialRoot\Settings\plugins"
Copy-Item -LiteralPath (Join-Path $InstalledRoot 'Settings\plugins\OpenRGBEffectsPlugin.dll') -Destination "$trialRoot\Settings\plugins"
# The Effects Plugin migrates its legacy settings on startup. Preserve that
# source tree, including empty directories, rather than copying only its DLL.
$effectSettings=Join-Path $InstalledRoot 'Settings\plugins\settings'
if(Test-Path -LiteralPath $effectSettings){Copy-Item -LiteralPath $effectSettings -Destination "$trialRoot\Settings\plugins" -Recurse}
New-Item -ItemType Directory -Path "$trialRoot\Settings\plugins\settings\effect-profiles" -Force | Out-Null
foreach($name in @('OpenRGB.json','Configuration.json')){
    $source=Join-Path $InstalledRoot "Settings\$name"
    if(Test-Path -LiteralPath $source){Copy-Item -LiteralPath $source -Destination "$trialRoot\Settings"}
}
$profiles=Get-ChildItem -LiteralPath (Join-Path $InstalledRoot 'Settings\profiles') -Filter '*.json' -File
$profiles | Copy-Item -Destination "$trialRoot\Settings\profiles"
$settingsPath=Join-Path $trialRoot 'Settings\OpenRGB.json'
$settings=Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json -AsHashtable
$settings['BatteryStatus']=@{enabled=$true;notifications=$false}
$settings['BatteryStatusHardware']=@{corsair_notifications_validated=$true}
$settings | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $settingsPath -Encoding utf8
$manifest=@{
    base='cb1385facc3337b0db7f9956624dbc111a80be38'
    sourceCommit=(& git -C $repoRoot rev-parse HEAD)
    installedExeSHA256=(Get-FileHash -LiteralPath (Join-Path $InstalledRoot 'OpenRGB-Custom.exe')).Hash
    trialExeSHA256=(Get-FileHash -LiteralPath "$trialRoot\OpenRGB-Battery-Trial.exe").Hash
    pluginSHA256=(Get-FileHash -LiteralPath "$trialRoot\Settings\plugins\OpenRGBBatteryStatusPlugin.dll").Hash
    profileCount=$profiles.Count
    corsairPassiveValidated=$true;asusAutomaticPollingEnabled=$false
    copiedProfiles=@($profiles | ForEach-Object {@{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
}
$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath "$trialRoot\trial-manifest.json" -Encoding utf8
Write-Output "Prepared $trialRoot; copied $($profiles.Count) profiles. Installed files unchanged. Do not run a hardware trial until the owner exits installed OpenRGB through its tray."
