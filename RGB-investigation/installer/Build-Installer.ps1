param(
    [string]$AppRoot = 'C:\Users\valerian\Apps\OpenRGB-Custom',
    [string]$Compiler = (Join-Path $PSScriptRoot 'tools\inno\ISCC.exe')
)
$ErrorActionPreference='Stop'
$payload=Join-Path $PSScriptRoot 'payload'
New-Item -ItemType Directory -Path $payload -Force | Out-Null
# Explicit allowlist: never package old executables, logs or diagnostic backups.
Get-ChildItem -LiteralPath $AppRoot -File | Where-Object { $_.Extension -in '.dll','.bin' -or $_.Name -eq 'OpenRGB-Custom.exe' } | Copy-Item -Destination $payload -Force
foreach($name in @('generic','iconengines','imageformats','networkinformation','platforms','styles','tls')){
    Copy-Item -LiteralPath (Join-Path $AppRoot $name) -Destination $payload -Recurse -Force
}
$settings=Join-Path $payload 'Settings'
New-Item -ItemType Directory -Path (Join-Path $settings 'plugins'),(Join-Path $settings 'profiles') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $AppRoot 'Settings\plugins\OpenRGBEffectsPlugin.dll') -Destination (Join-Path $settings 'plugins') -Force
Copy-Item -LiteralPath (Join-Path $AppRoot 'Settings\plugins\OpenRGBBatteryStatusPlugin.dll') -Destination (Join-Path $settings 'plugins') -Force
Copy-Item -LiteralPath (Join-Path $AppRoot 'Settings\OpenRGB.json') -Destination $settings -Force
Get-ChildItem -LiteralPath (Join-Path $AppRoot 'Settings\profiles') -Filter '*.json' -File | Copy-Item -Destination (Join-Path $settings 'profiles') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Configure-Startup.ps1') -Destination $payload -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Configure-Mouse-Idle.ps1') -Destination $payload -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Migrate-Device-Profiles.ps1') -Destination $payload -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'SLInfinity-Layout.json') -Destination $payload -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'GalaxGPU-Layout.json') -Destination $payload -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Start-OpenRGB-Custom.ps1') -Destination $payload -Force
Copy-Item -LiteralPath 'C:\Users\valerian\git-repos\OpenRGB-KFA2\LICENSE' -Destination (Join-Path $payload 'LICENSE-OpenRGB.txt') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'INSTALL-NOTES.txt') -Destination $payload -Force
$manifest=Get-ChildItem -LiteralPath $payload -File -Recurse | ForEach-Object {
    [pscustomobject]@{File=$_.FullName.Substring($payload.Length+1);SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'payload-manifest.json') -Encoding utf8
& $Compiler (Join-Path $PSScriptRoot 'OpenRGB-Custom.iss')
if($LASTEXITCODE -ne 0){throw 'Installer compilation failed'}
