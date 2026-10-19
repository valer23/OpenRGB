$ErrorActionPreference='Stop'
$packagingRepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source=Get-Content -LiteralPath (Join-Path $packagingRepoRoot 'scripts\Prepare-Battery-Trial.ps1') -Raw
$source=$source.Replace('$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot ''..'')).Path','$repoRoot=$packagingRepoRoot')
$source=$source.Replace('$trialRoot=Join-Path $repoRoot ''build-battery\trial''','$trialRoot=$packagingTrialRoot')
$source=$source.Replace('$coreExe=Join-Path $repoRoot ''build-battery\core\release\OpenRGB.exe''','$coreExe=$packagingCoreFixture')
$source=$source.Replace('$pluginDll=Join-Path $repoRoot ''build-battery\plugin\OpenRGBBatteryStatusPlugin.dll''','$pluginDll=$packagingPluginFixture')
$prepare=[scriptblock]::Create($source)
foreach($hasLegacySettings in @($true,$false)){
    $root=Join-Path $packagingRepoRoot ('build-battery\tests\packaging-fixtures\'+[guid]::NewGuid().ToString())
    $installed=Join-Path $root 'installed';$packagingTrialRoot=Join-Path $root 'trial'
    New-Item -ItemType Directory -Path (Join-Path $installed 'Settings\plugins'),(Join-Path $installed 'Settings\profiles') -Force | Out-Null
    'fixture' | Set-Content -LiteralPath (Join-Path $installed 'OpenRGB-Custom.exe')
    $packagingCoreFixture=Join-Path $root 'core.exe';$packagingPluginFixture=Join-Path $root 'OpenRGBBatteryStatusPlugin.dll'
    'not executed' | Set-Content -LiteralPath $packagingCoreFixture
    'not executed' | Set-Content -LiteralPath $packagingPluginFixture
    'not executed' | Set-Content -LiteralPath (Join-Path $installed 'Settings\plugins\OpenRGBEffectsPlugin.dll')
    '{"CorsairMouseIdle":{"enabled":true,"timeout_seconds":60},"OtherSetting":{"keep":42}}' | Set-Content -LiteralPath (Join-Path $installed 'Settings\OpenRGB.json')
    '{"name":"preserved-profile"}' | Set-Content -LiteralPath (Join-Path $installed 'Settings\profiles\scene.json')
    if($hasLegacySettings){
        New-Item -ItemType Directory -Path (Join-Path $installed 'Settings\plugins\settings\effect-profiles') -Force | Out-Null
        '{"effect":"test"}' | Set-Content -LiteralPath (Join-Path $installed 'Settings\plugins\settings\effect-profiles\effect.json')
        '{"enabled":true}' | Set-Content -LiteralPath (Join-Path $installed 'Settings\plugins\settings\EffectSettings.json')
    }
    & $prepare -InstalledRoot $installed | Out-Null
    if(-not(Test-Path -LiteralPath (Join-Path $packagingTrialRoot 'Settings\plugins\settings\effect-profiles'))){throw 'Effects migration source directory missing in trial'}
    foreach($relative in @('Settings\profiles\scene.json')+$(if($hasLegacySettings){@('Settings\plugins\settings\effect-profiles\effect.json','Settings\plugins\settings\EffectSettings.json')}else{@()})){
        if((Get-FileHash -LiteralPath (Join-Path $installed $relative)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $packagingTrialRoot $relative)).Hash){throw "Trial did not preserve $relative"}
    }
    $settings=Get-Content -LiteralPath (Join-Path $packagingTrialRoot 'Settings\OpenRGB.json') -Raw | ConvertFrom-Json -AsHashtable
    if($settings.CorsairMouseIdle.timeout_seconds -ne 60 -or $settings.OtherSetting.keep -ne 42){throw 'Non-battery settings changed'}
}
Write-Host 'Trial packaging: profiles, legacy effects settings and absent-source migration directory preserved.'
