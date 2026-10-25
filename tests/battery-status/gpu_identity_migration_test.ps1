param([string]$InstallerRoot=(Join-Path $PSScriptRoot '..\..\RGB-investigation\installer'))
$ErrorActionPreference='Stop'
$InstallerRoot=(Resolve-Path $InstallerRoot).Path
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('openrgb-gpu-identity-'+[guid]::NewGuid())
New-Item -ItemType Directory -Path (Join-Path $fixture 'profiles') -Force | Out-Null
$name='KFA2 GeForce RTX 3080 Ti SG 1-Click OC'
$oldModes=@([ordered]@{name='Direct';value=1;flags=32;color_mode=1;colors=@();speed=0;direction=0;brightness=0},[ordered]@{name='Breathing';value=2;flags=32;color_mode=1;colors=@();speed=0;direction=0;brightness=0})
$gpu=[ordered]@{name=$name;description='GALAX / KFA2 RTX GPU';vendor='GALAX';location='I2C: Nvidia NvAPI I2C on GPU 0, address 0x23';serial='';version='';active_mode=0;modes=$oldModes;colors=@(1149457);zones=@(@{name='GPU';leds_count=1});custom_preserve='untouched'}
$zone=[ordered]@{name=$name;description=$gpu.description;vendor=$gpu.vendor;location=$gpu.location;serial='';version='';zone_idx=0;self_brightness=73}
$profile=[ordered]@{profile_name='GPU regression';profile_version=5;controllers=@($gpu);plugins=@{'OpenRGB Effects Plugin'=@{Effects=@(@{ControllerZones=@($zone);CustomSettings=@{layer='original';colour=982399}})}}}
$file=Join-Path $fixture 'profiles\GPU.json'
$profile|ConvertTo-Json -Depth 30|Set-Content -LiteralPath $file -Encoding UTF8
$legacy=$profile|ConvertTo-Json -Depth 30|ConvertFrom-Json
$legacy.controllers[0].name='KFA2 GeForce RTX 3080 Ti EX Gamer'
$legacy.controllers[0].description='Captured KFA2 EX lighting protocol (Windows NVAPI)'
$legacy.controllers[0].modes[0].name='Static'
$legacy.plugins.'OpenRGB Effects Plugin'.Effects[0].ControllerZones[0].name=$legacy.controllers[0].name
$legacy.plugins.'OpenRGB Effects Plugin'.Effects[0].ControllerZones[0].description=$legacy.controllers[0].description
$legacyFile=Join-Path $fixture 'profiles\LegacyEX.json'
$legacy|ConvertTo-Json -Depth 30|Set-Content -LiteralPath $legacyFile -Encoding UTF8
$foreign=$profile|ConvertTo-Json -Depth 30|ConvertFrom-Json
$foreign.controllers[0].name='Other GPU';$foreign.plugins=[pscustomobject]@{}
$foreignFile=Join-Path $fixture 'profiles\Other.json'
$foreign|ConvertTo-Json -Depth 30|Set-Content -LiteralPath $foreignFile -Encoding UTF8
$foreignHash=(Get-FileHash -LiteralPath $foreignFile).Hash
$before=(Get-FileHash -LiteralPath $file).Hash
& (Join-Path $InstallerRoot 'Migrate-Device-Profiles.ps1') -SettingsRoot $fixture
$updated=Get-Content -LiteralPath $file -Raw|ConvertFrom-Json
if($updated.controllers[0].vendor -ne 'KFA2' -or $updated.controllers[0].description -ne 'KFA2 SG I2C lighting controller'){throw 'Already-renamed SG still has legacy identity'}
if($updated.plugins.'OpenRGB Effects Plugin'.Effects[0].ControllerZones[0].vendor -ne 'KFA2'){throw 'Effects assignment identity was not migrated'}
if($updated.controllers[0].colors[0] -ne 1149457 -or $updated.controllers[0].custom_preserve -ne 'untouched' -or $updated.plugins.'OpenRGB Effects Plugin'.Effects[0].CustomSettings.layer -ne 'original' -or $updated.plugins.'OpenRGB Effects Plugin'.Effects[0].ControllerZones[0].self_brightness -ne 73){throw 'GPU colour/effect settings changed'}
if($updated.controllers[0].modes.Count -ne 2 -or $updated.controllers[0].modes[1].name -ne 'Motherboard Sync'){throw 'GPU modes do not match actual SG driver'}
$backups=@(Get-ChildItem -LiteralPath (Join-Path $fixture 'profile-backups\before-sg-upgrade') -Filter 'GPU.json' -Recurse -File)
if($backups.Count -ne 1 -or (Get-FileHash -LiteralPath $backups[0].FullName).Hash -ne $before){throw 'Original profile backup missing or mismatched'}
$legacyUpdated=Get-Content -LiteralPath $legacyFile -Raw|ConvertFrom-Json
if($legacyUpdated.controllers[0].name -ne $name -or $legacyUpdated.controllers[0].vendor -ne 'KFA2' -or $legacyUpdated.controllers[0].modes[0].name -ne 'Direct' -or $legacyUpdated.plugins.'OpenRGB Effects Plugin'.Effects[0].ControllerZones[0].vendor -ne 'KFA2'){throw 'Legacy EX alias migration failed'}
if((Get-FileHash -LiteralPath $foreignFile).Hash -ne $foreignHash){throw 'Unrelated GPU profile changed'}
$after=(Get-FileHash -LiteralPath $file).Hash
& (Join-Path $InstallerRoot 'Migrate-Device-Profiles.ps1') -SettingsRoot $fixture
if((Get-FileHash -LiteralPath $file).Hash -ne $after){throw 'Migration is not idempotent'}
# Unsupported old hardware effects must fail before changing any file.
$invalid=Get-Content -LiteralPath $file -Raw|ConvertFrom-Json
$invalid.controllers[0].vendor='GALAX';$invalid.controllers[0].description='GALAX / KFA2 RTX GPU';$invalid.controllers[0].modes[0].name='Breathing'
$invalid|ConvertTo-Json -Depth 30|Set-Content -LiteralPath $file -Encoding UTF8
$invalidHash=(Get-FileHash -LiteralPath $file).Hash
$refused=$false
try {& (Join-Path $InstallerRoot 'Migrate-Device-Profiles.ps1') -SettingsRoot $fixture}catch {$refused=$true}
if(-not $refused -or (Get-FileHash -LiteralPath $file).Hash -ne $invalidHash){throw 'Unsupported GPU mode was not rejected atomically'}
Write-Output "GPU identity migration: core/effect bindings, preserved colours/effects, verified backup, idempotence and unsupported-mode refusal passed. Fixture: $fixture"