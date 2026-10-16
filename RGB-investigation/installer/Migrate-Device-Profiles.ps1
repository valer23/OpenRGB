param([string]$SettingsRoot = (Join-Path $PSScriptRoot 'Settings'))
$ErrorActionPreference = 'Stop'
$layout = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'SLInfinity-Layout.json') -Raw | ConvertFrom-Json
$oldGpu = 'KFA2 GeForce RTX 3080 Ti EX Gamer'
$newGpu = 'KFA2 GeForce RTX 3080 Ti SG 1-Click OC'
$oldFan = 'Lian Li SL Infinity (local static test)'
$newFan = 'Lian Li Uni Hub - SL Infinity'
$script:changed = $false

function Get-ProfileHash([string]$path) {
    $stream = [IO.File]::OpenRead($path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '')} finally {$stream.Dispose(); $sha.Dispose()}
}

function Update-DeviceReferences($node) {
    if($node -is [System.Management.Automation.PSCustomObject]) {
        if($node.name -eq $oldGpu) {
            $node.name = $newGpu
            if($node.PSObject.Properties['description']) {$node.description = 'KFA2 SG I2C lighting controller'}
            if($node.PSObject.Properties['modes'] -and $node.modes.Count -gt 0 -and $node.modes[0].name -eq 'Static') {
                $node.modes[0].name = 'Direct'
            }
            $script:changed = $true
        }
        if($node.name -eq $oldFan) {
            $node.name = $newFan
            if($node.PSObject.Properties['description']) {$node.description = $layout.description}
            if($node.PSObject.Properties['modes'] -and $node.PSObject.Properties['zones']) {
                $oldModes = @($node.modes)
                $activeName = $oldModes[[int]$node.active_mode].name
                $newModes = @(foreach($template in $layout.modes) {
                    $template | ConvertTo-Json -Depth 100 | ConvertFrom-Json
                })
                foreach($oldMode in $oldModes) {
                    $targetName = if($oldMode.name -eq 'Static') {'Custom'} else {$oldMode.name}
                    $newMode = @($newModes | Where-Object {$_.name -eq $targetName})
                    if($newMode.Count -ne 1) {throw ('Cannot map fan mode: ' + $oldMode.name)}
                    $target = $newMode[0]
                    foreach($field in @('speed','direction','color_mode','colors')) {
                        if($oldMode.PSObject.Properties[$field] -and $target.PSObject.Properties[$field]) {$target.$field = $oldMode.$field}
                    }
                    if($oldMode.brightness_max -gt 0 -and $target.brightness_max -gt 0) {
                        $target.brightness = [int][Math]::Round($oldMode.brightness * $target.brightness_max / $oldMode.brightness_max)
                    }
                }
                $targetName = if($activeName -eq 'Static') {'Custom'} else {$activeName}
                $node.modes = $newModes
                $mapped = $false
                for($i=0; $i -lt $newModes.Count; $i++) {
                    if($newModes[$i].name -eq $targetName) {$node.active_mode = $i; $mapped = $true; break}
                }
                if(-not $mapped) {throw ('Cannot map active fan mode: ' + $activeName)}
                if($node.zones.Count -ne $layout.zone_names.Count) {throw 'Unexpected fan zone count; profiles were not modified.'}
                for($i=0; $i -lt $node.zones.Count; $i++) {
                    if($node.zones[$i].name -match '^Port [1-4] (centres|sides)$') {$node.zones[$i].name = $layout.zone_names[$i]}
                }
            }
            $script:changed = $true
        }
        foreach($property in @($node.PSObject.Properties)) {Update-DeviceReferences $property.Value}
    } elseif($node -is [System.Collections.IList]) {
        foreach($entry in $node) {Update-DeviceReferences $entry}
    }
}

# Parse and validate every candidate before any file is changed.
$files = @(Get-ChildItem -LiteralPath (Join-Path $SettingsRoot 'profiles') -Filter '*.json' -File -ErrorAction SilentlyContinue)
$configuration = Join-Path $SettingsRoot 'Configuration.json'
if(Test-Path -LiteralPath $configuration) {$files += Get-Item -LiteralPath $configuration}
$plan = @()
foreach($file in $files) {
    $original = [IO.File]::ReadAllText($file.FullName)
    $document = $original | ConvertFrom-Json
    $script:changed = $false
    Update-DeviceReferences $document
    if($script:changed) {
        $text = $document | ConvertTo-Json -Depth 100
        $null = $text | ConvertFrom-Json
        $relative = if($file.DirectoryName -eq (Join-Path $SettingsRoot 'profiles')) {'profiles\' + $file.Name} else {$file.Name}
        $plan += [pscustomobject]@{Path=$file.FullName;Relative=$relative;Text=$text;Hash=(Get-ProfileHash $file.FullName);Existed=$true}
    }
}
# Lighting profiles restore state, not channel configuration. Seed only this
# installation's known hub from ValWhite if persistent configuration lacks it.
$whitePath = Join-Path $SettingsRoot 'profiles\ValWhite.json'
if(Test-Path -LiteralPath $whitePath) {
    $whitePlan = @($plan | Where-Object {$_.Path -eq $whitePath})
    $white = if($whitePlan.Count) {$whitePlan[0].Text | ConvertFrom-Json} else {Get-Content -LiteralPath $whitePath -Raw | ConvertFrom-Json}
    $fans = @($white.controllers | Where-Object {$_.name -eq $newFan})
    if($fans.Count -gt 1) {throw 'Multiple fan hubs in ValWhite require explicit configuration; files were not modified.'}
    if($fans.Count -eq 1) {
        $configPlan = @($plan | Where-Object {$_.Path -eq $configuration})
        $configExists = Test-Path -LiteralPath $configuration
        $config = if($configPlan.Count) {$configPlan[0].Text | ConvertFrom-Json} elseif($configExists) {Get-Content -LiteralPath $configuration -Raw | ConvertFrom-Json} else {
            [pscustomobject]@{profile_name='Controller Configuration';profile_version=$white.profile_version;controllers=@()}
        }
        $fan = $fans[0]
        $configured = @($config.controllers | Where-Object {$_.name -eq $fan.name -and $_.location -eq $fan.location -and $_.serial -eq $fan.serial})
        if($configured.Count -eq 0) {
            foreach($zone in $fan.zones) {$zone.flags = [int]$zone.flags -bor 4096} # ZONE_FLAG_MANUALLY_CONFIGURED_SIZE
            $config.controllers = @($config.controllers) + @($fan)
            $text = $config | ConvertTo-Json -Depth 100
            if($configPlan.Count) {$configPlan[0].Text = $text} else {
                $hash = if($configExists) {Get-ProfileHash $configuration} else {$null}
                $plan += [pscustomobject]@{Path=$configuration;Relative='Configuration.json';Text=$text;Hash=$hash;Existed=$configExists}
            }
        }
    }
}
if($plan.Count -eq 0) {Write-Output 'Device profile identities already current.'; return}
$backupRoot = Join-Path $SettingsRoot ('profile-backups\before-sg-upgrade\' + [guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
@($plan | Select-Object Relative,Existed,Hash) | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $backupRoot 'migration-manifest.json') -Encoding UTF8
foreach($entry in $plan) {
    if(-not $entry.Existed) {continue}
    $backup = Join-Path $backupRoot $entry.Relative
    New-Item -ItemType Directory -Path (Split-Path $backup) -Force | Out-Null
    Copy-Item -LiteralPath $entry.Path -Destination $backup
    if((Get-ProfileHash $backup) -ne $entry.Hash) {throw 'Profile backup verification failed.'}
}
$written = @()
try {
    foreach($entry in $plan) {
        if($entry.Existed) {
            if((Get-ProfileHash $entry.Path) -ne $entry.Hash) {throw 'A profile changed during migration; retry after closing OpenRGB.'}
        } elseif(Test-Path -LiteralPath $entry.Path) {throw 'Controller configuration appeared during migration; retry after closing OpenRGB.'}
        $temp = $entry.Path + '.' + [guid]::NewGuid().ToString() + '.tmp'
        try {
            [IO.File]::WriteAllText($temp, $entry.Text, [Text.UTF8Encoding]::new($false))
            if($entry.Existed) {[IO.File]::Replace($temp, $entry.Path, [NullString]::Value)} else {[IO.File]::Move($temp, $entry.Path)}
            $written += $entry
        } finally {
            if(Test-Path -LiteralPath $temp) {Remove-Item -LiteralPath $temp}
        }
    }
} catch {
    foreach($entry in $written) {
        if($entry.Existed) {Copy-Item -LiteralPath (Join-Path $backupRoot $entry.Relative) -Destination $entry.Path -Force} else {Remove-Item -LiteralPath $entry.Path}
    }
    throw
}
Write-Output ('Migrated ' + $plan.Count + ' files. Originals: ' + $backupRoot)
