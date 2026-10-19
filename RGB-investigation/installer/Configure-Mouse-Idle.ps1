param([string]$SettingsRoot = (Join-Path $PSScriptRoot 'Settings'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Web.Extensions
$parser = New-Object System.Web.Script.Serialization.JavaScriptSerializer
$parser.MaxJsonLength = [int]::MaxValue
$appPath = Join-Path $SettingsRoot 'OpenRGB.json'
$devicePath = Join-Path $SettingsRoot 'Configuration.json'
$app = $parser.DeserializeObject([IO.File]::ReadAllText($appPath))
$legacy = if($app.ContainsKey('CorsairMouseIdle')) {$app['CorsairMouseIdle']} else {$null}
$devices = if(Test-Path -LiteralPath $devicePath) {$parser.DeserializeObject([IO.File]::ReadAllText($devicePath))} else {$parser.DeserializeObject('{"profile_name":"Controller Configuration","profile_version":6,"controllers":[]}')}
$mouseName = 'Corsair Slipstream Wireless Receiver HW'
$matches = @($devices['controllers'] | Where-Object {$_['name'] -eq $mouseName})
if($matches.Count -gt 1){throw 'Ambiguous saved mouse configuration; no settings changed.'}
$mouse = if($matches.Count -eq 1) {$matches[0]} else {$null}
if(-not $mouse) {
    $snapshotPath = Join-Path $SettingsRoot 'profiles\ValWhite.json'
    if(-not (Test-Path -LiteralPath $snapshotPath)){throw 'Mouse snapshot is missing; no settings changed.'}
    $snapshot = $parser.DeserializeObject([IO.File]::ReadAllText($snapshotPath))
    $matches = @($snapshot['controllers'] | Where-Object {$_['name'] -eq $mouseName})
    if($matches.Count -ne 1){throw 'Expected exactly one wireless mouse snapshot; no settings changed.'}
    $mouse = $matches[0]
    $devices['controllers'] = @($devices['controllers']) + @($mouse)
}
$hasNewSettings = $mouse.ContainsKey('configuration') -and $mouse['configuration'].ContainsKey('configuration') -and $mouse['configuration']['configuration'].ContainsKey('idle_handoff_enabled')
$changed = $false
if($hasNewSettings) {
    $current = $mouse['configuration']['configuration']
    $timeoutValue = $current['idle_timeout_seconds']
    if($current['idle_handoff_enabled'] -isnot [bool] -or
       -not $current.ContainsKey('idle_timeout_seconds') -or
       ($timeoutValue -isnot [int] -and $timeoutValue -isnot [long]) -or
       $timeoutValue -lt 5 -or $timeoutValue -gt 3600) {
        throw 'Invalid saved device idle settings; no settings changed.'
    }
    $requiredFlags = [long]$mouse['flags'] -bor (1 -shl 17) -bor (1 -shl 25)
    if($requiredFlags -ne $mouse['flags']) {$mouse['flags'] = $requiredFlags;$changed = $true}
}
if(-not $hasNewSettings) {
    $enabled = $true
    $timeout = 60
    if($null -ne $legacy) {
        if($legacy['enabled'] -isnot [bool]){throw 'Invalid legacy idle toggle; no settings changed.'}
        $enabled = $legacy['enabled']
        if($legacy.ContainsKey('timeout_seconds')) {
            $value = $legacy['timeout_seconds']
            if(($value -isnot [int] -and $value -isnot [long]) -or $value -lt 5 -or $value -gt 3600){throw 'Invalid legacy idle timeout; no settings changed.'}
            $timeout = $value
        } else {$timeout = 900}
    }
    if(-not $mouse.ContainsKey('configuration')) {$mouse['configuration'] = $parser.DeserializeObject('{"configuration":{}}')}
    if(-not $mouse['configuration'].ContainsKey('configuration')) {$mouse['configuration']['configuration'] = $parser.DeserializeObject('{}')}
    $mouse['configuration']['configuration']['idle_handoff_enabled'] = $enabled
    $mouse['configuration']['configuration']['idle_timeout_seconds'] = $timeout
    $mouse['flags'] = [long]$mouse['flags'] -bor (1 -shl 17) -bor (1 -shl 25)
    $changed = $true
}
if(-not $changed -and $null -eq $legacy){return}
$backupRoot = Join-Path $SettingsRoot ('profile-backups\before-device-idle-config\'+[guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
function Save-Atomic($path,$document) {
    $raw = ConvertTo-Json -InputObject $document -Depth 100
    $null = $parser.DeserializeObject($raw)
    $temporary = $path+'.'+[guid]::NewGuid().ToString()+'.tmp'
    try {
        [IO.File]::WriteAllText($temporary,$raw,[Text.UTF8Encoding]::new($false))
        if(Test-Path -LiteralPath $path){[IO.File]::Replace($temporary,$path,(Join-Path $backupRoot (Split-Path $path -Leaf)))}else{[IO.File]::Move($temporary,$path)}
    } finally {if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}}
}
# Save the device entry before removing the old application-level property.
if($changed){Save-Atomic $devicePath $devices}
if($null -ne $legacy){$null = $app.Remove('CorsairMouseIdle');Save-Atomic $appPath $app}
