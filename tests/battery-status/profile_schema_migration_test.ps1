param([string]$InstallerRoot=(Join-Path $PSScriptRoot '..\..\RGB-investigation\installer'))
$ErrorActionPreference='Stop'
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('openrgb-profile-schema-'+[guid]::NewGuid())
New-Item -ItemType Directory -Path (Join-Path $fixture 'profiles') -Force|Out-Null
$board=[ordered]@{name='ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)';description='Onboard logo only; captured 0B05:1CD2 protocol';vendor='ASUS';serial='';location='HID: existing-board';active_mode=0;colors=@(1122867);modes=@(@{name='Static';flags=32;color_mode=1});configuration='{"shutdown_effect":"Off","shutdown_writer":"ASUS service","shutdown_color":"A600FF"}'}
$profile=[ordered]@{profile_name='Motherboard schema';controllers=@($board);plugins=@{effect=@{colour=98765;brightness=61}}}
$file=Join-Path $fixture 'profiles\Motherboard.json'
$profile|ConvertTo-Json -Depth 20|Set-Content -LiteralPath $file -Encoding UTF8
$before=(Get-FileHash -LiteralPath $file).Hash
& (Join-Path $InstallerRoot 'Migrate-Device-Profiles.ps1') -SettingsRoot $fixture
$updated=Get-Content -LiteralPath $file -Raw|ConvertFrom-Json
if($updated.controllers.Count -ne 1 -or $updated.controllers[0].modes[0].flags -ne 288){throw 'Motherboard saved mode does not match current manual-save capability'}
if($updated.controllers[0].colors[0] -ne 1122867 -or $updated.controllers[0].configuration -cne $board.configuration -or $updated.plugins.effect.colour -ne 98765){throw 'Motherboard colours, shutdown preference or effect data changed'}
$backup=@(Get-ChildItem -LiteralPath (Join-Path $fixture 'profile-backups') -Filter 'Motherboard.json' -Recurse -File)
if($backup.Count -ne 1 -or (Get-FileHash -LiteralPath $backup[0].FullName).Hash -ne $before){throw 'Motherboard backup was not verified'}
$hash=(Get-FileHash -LiteralPath $file).Hash
& (Join-Path $InstallerRoot 'Migrate-Device-Profiles.ps1') -SettingsRoot $fixture
if((Get-FileHash -LiteralPath $file).Hash -ne $hash){throw 'Motherboard schema migration is not idempotent'}
$second=$updated.controllers[0]|ConvertTo-Json -Depth 20|ConvertFrom-Json
$second.serial='another physical board';$second.location='HID: another-board';$second.modes[0].flags=32
$updated.controllers+=@($second)
$updated|ConvertTo-Json -Depth 20|Set-Content -LiteralPath $file -Encoding UTF8
& (Join-Path $InstallerRoot 'Migrate-Device-Profiles.ps1') -SettingsRoot $fixture
$two=Get-Content -LiteralPath $file -Raw|ConvertFrom-Json
if($two.controllers.Count -ne 2 -or $two.controllers[1].serial -ne 'another physical board'){throw 'Distinct same-name saved controllers were collapsed'}
$two.controllers[0].modes[0].flags=64
$two|ConvertTo-Json -Depth 20|Set-Content -LiteralPath $file -Encoding UTF8
$invalidHash=(Get-FileHash -LiteralPath $file).Hash;$refused=$false
try {& (Join-Path $InstallerRoot 'Migrate-Device-Profiles.ps1') -SettingsRoot $fixture}catch {$refused=$true}
if(-not $refused -or (Get-FileHash -LiteralPath $file).Hash -ne $invalidHash){throw 'Unknown motherboard schema was not refused before writing'}
Write-Output 'Motherboard profile schema: capability upgrade, preserved effects/Off preference, verified backup, idempotence, distinct-device retention and unknown-schema refusal passed.'