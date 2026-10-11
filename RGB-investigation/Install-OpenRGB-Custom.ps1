$ErrorActionPreference = 'Stop'
$buildFiles = Join-Path $PSScriptRoot 'OpenRGB-Custom-full-build\release'
$appRoot = 'C:\Users\valerian\Apps\OpenRGB-Custom'
$settingsRoot = 'C:\Users\valerian\AppData\Local\OpenRGB-Custom'
$testSettings = Join-Path $PSScriptRoot 'OpenRGB-KFA2-settings'
if(-not(Test-Path -LiteralPath (Join-Path $buildFiles 'OpenRGB-Custom.exe'))){throw 'The full build is not ready.'}
if(Get-Process OpenRGB,OpenRGB-Custom,OpenRGB-KFA2-test -ErrorAction SilentlyContinue){throw 'Close all OpenRGB instances before installing.'}
New-Item -ItemType Directory -Path $appRoot -Force | Out-Null
New-Item -ItemType Directory -Path $settingsRoot -Force | Out-Null
Copy-Item -Path (Join-Path $buildFiles '*') -Destination $appRoot -Recurse -Force
if(-not(Test-Path -LiteralPath (Join-Path $settingsRoot 'OpenRGB.json'))){
    Copy-Item -LiteralPath (Join-Path $testSettings 'OpenRGB.json') -Destination $settingsRoot
    Copy-Item -LiteralPath (Join-Path $testSettings 'profiles') -Destination $settingsRoot -Recurse
}
$settingsFile = Join-Path $settingsRoot 'OpenRGB.json'
$config = Get-Content -LiteralPath $settingsFile -Raw | ConvertFrom-Json
$config | Add-Member -NotePropertyName AutoStart -NotePropertyValue ([PSCustomObject]@{enabled=$false;start_minimized=$true;custom_arguments=''}) -Force
$config | ConvertTo-Json -Depth 50 | Set-Content -LiteralPath $settingsFile -Encoding utf8
$launcher = Join-Path $appRoot 'OpenRGB-Custom.cmd'
$launcherText = @'
@echo off
powershell.exe -NoProfile -Command "Start-Process -FilePath 'C:\Users\valerian\Apps\OpenRGB-Custom\OpenRGB-Custom.exe' -Verb RunAs -WorkingDirectory 'C:\Users\valerian\Apps\OpenRGB-Custom' -ArgumentList '--config C:\Users\valerian\AppData\Local\OpenRGB-Custom --gui'"
'@
Set-Content -LiteralPath $launcher -Value $launcherText -Encoding ascii
$startMenu = 'C:\Users\valerian\AppData\Roaming\Microsoft\Windows\Start Menu\Programs'
$shell = New-Object -ComObject WScript.Shell
$link = $shell.CreateShortcut((Join-Path $startMenu 'OpenRGB Custom.lnk'))
$link.TargetPath = Join-Path $appRoot 'OpenRGB-Custom.exe'
$link.Arguments = '--config "C:\Users\valerian\AppData\Local\OpenRGB-Custom" --gui'
$link.WorkingDirectory = $appRoot
$link.IconLocation = (Join-Path $appRoot 'OpenRGB-Custom.exe') + ',0'
$link.Description = 'OpenRGB with local GPU and fan support'
$link.Save()
$shortcutPath = Join-Path $startMenu 'OpenRGB Custom.lnk'
$shortcutBytes = [IO.File]::ReadAllBytes($shortcutPath)
if($shortcutBytes.Length -lt 76){throw 'Invalid Start menu shortcut.'}
$shortcutBytes[0x15] = $shortcutBytes[0x15] -bor 0x20
[IO.File]::WriteAllBytes($shortcutPath, $shortcutBytes)
Write-Host ('Installed app: ' + $appRoot)
Write-Host ('Settings: ' + $settingsRoot)
Write-Host 'Start menu shortcut: OpenRGB Custom'
