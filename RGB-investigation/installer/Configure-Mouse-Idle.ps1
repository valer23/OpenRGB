param([string]$SettingsRoot = (Join-Path $PSScriptRoot 'Settings'))
$ErrorActionPreference = 'Stop'
$configPath = Join-Path $SettingsRoot 'OpenRGB.json'
if(-not (Test-Path -LiteralPath $configPath)) {throw 'OpenRGB settings are missing.'}

# Windows PowerShell's ConvertFrom-Json rejects this application's case-variant
# keys. Validate with a case-sensitive dictionary and preserve the original text.
Add-Type -AssemblyName System.Web.Extensions
$parser = New-Object System.Web.Script.Serialization.JavaScriptSerializer
$parser.MaxJsonLength = [int]::MaxValue
$raw = [IO.File]::ReadAllText($configPath)
$document = $parser.DeserializeObject($raw)
if($document -isnot [System.Collections.Generic.Dictionary[string,object]]) {throw 'Expected an OpenRGB settings object.'}
if($document.ContainsKey('CorsairMouseIdle')) {
    # Keep an existing opt-out or custom timeout on subsequent upgrades.
    return
}

# This personal package enables only the supported wireless Dark Core mouse.
$property = '"CorsairMouseIdle": {"enabled": true, "timeout_seconds": 60}'
$separator = if($document.Count) {','} else {''}
$updated = [regex]::Replace($raw, '\A(\s*\{)', ('$1' + "`r`n  " + $property + $separator))
$check = $parser.DeserializeObject($updated)
if(-not $check.ContainsKey('CorsairMouseIdle')) {throw 'Could not add mouse idle settings.'}
$backupRoot = Join-Path $SettingsRoot 'profile-backups\before-mouse-idle-upgrade'
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
$backup = Join-Path $backupRoot ('OpenRGB.' + [guid]::NewGuid().ToString() + '.json')
$temporary = $configPath + '.' + [guid]::NewGuid().ToString() + '.tmp'
try {
    [IO.File]::WriteAllText($temporary, $updated, [Text.UTF8Encoding]::new($false))
    [IO.File]::Replace($temporary, $configPath, $backup)
} finally {
    if(Test-Path -LiteralPath $temporary) {Remove-Item -LiteralPath $temporary}
}
